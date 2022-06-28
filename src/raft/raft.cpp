#include "raft/raft.h"

namespace flashpoint {

Raft::Raft()
    : running_(false),
      me_(-1),
      callbacks_{},
      current_term_(0),
      voted_for_(-1),
      log_offset_(0),
      commit_index_(0),
      last_applied_(0),
      last_heartbeat_{},
      role_(FOLLOWER) {}

Raft::~Raft() {
  if (running_)
    kill();
}

void Raft::serve() {
  running_ = true;
  ticker_thread_ = std::thread(&Raft::ticker, this);
}
void Raft::kill() {
  running_ = false;

  if (ticker_thread_.joinable())
    ticker_thread_.join();
}

void Raft::start(const std::string &log_id, const std::string &data) {

}

void Raft::registerPeer(std::string &peer_address) {
  auto channel = grpc::CreateChannel(peer_address, grpc::InsecureChannelCredentials());

  Peer peer = {
      .id = 0,
      .next_index = 0,
      .match_index = 0,
      .stub = protos::raft::Raft::NewStub(channel),
      .peer_latch = {},
  };
}

grpc::Status Raft::AppendEntries(::grpc::ServerContext *context,
                                 const ::protos::raft::AppendEntriesArgs *args,
                                 ::protos::raft::AppendEntriesReply *reply) {
  std::lock_guard<std::mutex> lock_guard(latch_);

  reply->set_term(current_term_);
  if (current_term_ < args->term())
    current_term_ = args->term();
  if (args->term() < current_term_) {
    reply->set_success(false);
    return grpc::Status::OK;
  }

  auto last_log_index = getLastLogEntry().log_index();
  if (last_log_index < args->prev_log_index()) {
    reply->set_conflict_index(last_log_index + 1);
    reply->set_conflict_term(-1);
    reply->set_success(false);
    return grpc::Status::OK;
  } else if (getLogEntry(args->prev_log_index()).log_term() != args->prev_log_index()) {
    reply->set_conflict_term(getLogEntry(args->prev_log_index()).log_term());
    auto conflict_index = getLastLogIndexOfTerm(args->prev_log_term());
    reply->set_conflict_index(conflict_index.has_value() ? conflict_index.value() : 0);
    reply->set_success(false);
    return grpc::Status::OK;
  }

  for (LogIndex i = args->prev_log_index(); i < last_log_index; i++)
    log_->pop_back();
  for (auto &entry : args->entries())
    log_->emplace_back(entry);

  commit_index_ = std::min(args->leader_commit_index(), getLastLogEntry().log_index());
  last_heartbeat_ = std::chrono::system_clock::now();

  reply->set_update_index(getLastLogEntry().log_index());
  reply->set_success(true);

  return grpc::Status::OK;
}
grpc::Status Raft::RequestVote(::grpc::ServerContext *context,
                               const ::protos::raft::RequestVoteArgs *args,
                               ::protos::raft::RequestVoteReply *reply) {
  std::lock_guard<std::mutex> lock_guard(latch_);

  auto last_log_entry = getLastLogEntry();
  reply->set_term(current_term_);

  if (current_term_ < args->term()) {
    current_term_ = args->term();
    voted_for_ = -1;
  }

  if (args->term() < current_term_ ||
      (voted_for_ != -1 && voted_for_ != args->candidate_id()) ||
      (args->last_log_term() < last_log_entry.log_term()) ||
      (args->last_log_term() == last_log_entry.log_term() && args->last_log_index() < last_log_entry.log_index())) {
    reply->set_vote_granted(false);
    return grpc::Status::OK;
  }

  last_heartbeat_ = std::chrono::system_clock::now();
  reply->set_vote_granted(true);
  voted_for_ = args->candidate_id();

  return grpc::Status::OK;
}

decltype(auto) Raft::appendEntries(int peer_id, const protos::raft::AppendEntriesArgs &args) {
  return std::async([this, peer_id, args]()
                        -> std::pair<std::unique_ptr<protos::raft::AppendEntriesReply>, Raft::Time> {
    std::unique_ptr<grpc::ClientAsyncResponseReader<protos::raft::AppendEntriesReply>> async_response_reader;
    grpc::ClientContext context = {};
    grpc::CompletionQueue completion_queue = {};

    {
      this->latch_.lock();
      auto &peer = this->peers_.at(peer_id);
      std::lock_guard<std::mutex> lock(peer.peer_latch);
      this->latch_.unlock();

      async_response_reader = peer.stub->AsyncAppendEntries(&context, args, &completion_queue);
    }

    auto reply = std::make_unique<protos::raft::AppendEntriesReply>();
    grpc::Status status;
    async_response_reader->Finish(reply.get(), &status, (void *) 1);

    if (status.ok()) {
      return {std::move(reply), std::chrono::system_clock::now()};
    } else {
      return {nullptr, std::chrono::system_clock::now()};
    }
  });
}
decltype(auto) Raft::requestVote(int peer_id, const protos::raft::RequestVoteArgs &args) {
  return std::async([this, peer_id, args]()
                        -> std::pair<std::unique_ptr<protos::raft::RequestVoteReply>, Raft::Time> {
    std::unique_ptr<grpc::ClientAsyncResponseReader<protos::raft::RequestVoteReply>> async_response_reader;
    grpc::ClientContext context = {};
    grpc::CompletionQueue completion_queue = {};

    {
      this->latch_.lock();
      auto &peer = this->peers_.at(peer_id);
      std::lock_guard<std::mutex> lock(peer.peer_latch);
      this->latch_.unlock();

      async_response_reader = peer.stub->AsyncRequestVote(&context, args, &completion_queue);
    }

    auto reply = std::make_unique<protos::raft::RequestVoteReply>();
    grpc::Status status;
    async_response_reader->Finish(reply.get(), &status, (void *) 1);

    if (status.ok()) {
      return {std::move(reply), std::chrono::system_clock::now()};
    } else {
      return {nullptr, std::chrono::system_clock::now()};
    }
  });
}

void Raft::ticker() {
  std::optional<std::future<LogTerm>> updater_status;
  std::optional<std::future<std::pair<LogTerm, bool>>> leader_election;
  leader_election = std::nullopt;
  LogTerm last_term = current_term_;

  while (running_) {
    auto current_time = std::chrono::system_clock::now();

    {
      std::lock_guard<std::mutex> lock_guard(latch_);

      if (last_term != current_term_) {
        if (updater_status.has_value())
          updater_status = std::nullopt;
        if (leader_election.has_value())
          leader_election = std::nullopt;
        role_ = FOLLOWER;
        last_term = current_term_;
        continue;
      }

      switch (role_) {
        case LEADER:
          if (!updater_status.has_value())
            updater_status = std::async(&Raft::updater, this);
          else if (updater_status->valid()) {
            current_term_ = updater_status->get();
            last_term = current_term_;
            role_ = FOLLOWER;
          }
          updateCommitIndex();
          break;

        case FOLLOWER:
          if (last_heartbeat_.time_since_epoch() > election_timeout_)
            role_ = CANDIDATE;
          break;

        case CANDIDATE:
          if (leader_election.has_value() && leader_election->valid()) {
            auto [term, won] = leader_election->get();
            if (current_term_ <= term && won) {
              role_ = LEADER;
              current_term_ = term;
              last_term = current_term_;
            } else {
              role_ = FOLLOWER;
            }

            leader_election = std::nullopt;
          } else if (!leader_election.has_value()) {
            leader_election = std::async(&Raft::beginLeaderElection, this);
          }
          break;
      }

      for (; last_applied_ < commit_index_; last_applied_++) {
        commitEntry(last_applied_);
      }
    }

    auto sleep_time = min_sleep_time_ + 0.5 * additional_sleep_time_;
    std::this_thread::sleep_until(current_time + sleep_time);
  }
}

Raft::LogTerm Raft::updater() {
  std::unordered_map<int, std::variant<Raft::Time, Raft::AppendEntriesResponse>> replies;

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(0, 1);

  while (running_) {
    std::set<int> reply_keys;
    for (auto &[key, _] : replies)
      reply_keys.insert(key);

    Raft::Time sleep_until = std::chrono::system_clock::now() + min_sleep_time_ + additional_sleep_time_;
    {
      std::lock_guard<std::mutex> lock_guard(latch_);

      for (auto &[peer_id, _] : peers_) {
        if (reply_keys.contains(peer_id))
          reply_keys.erase(peer_id);

        if (replies.contains(peer_id) &&
            (std::holds_alternative<Raft::Time>(replies[peer_id]) &&
                std::chrono::system_clock::now() < std::get<0>(replies[peer_id])) &&
            (std::holds_alternative<Raft::AppendEntriesResponse>(replies[peer_id]) &&
                !std::get<1>(replies[peer_id]).valid()))
          continue;

        if (replies.contains(peer_id) &&
            std::holds_alternative<Raft::AppendEntriesResponse>(replies[peer_id]) &&
            std::get<1>(replies[peer_id]).valid()) {
          const auto &[kReply, kReplyTime] = std::get<1>(replies[peer_id]).get();

          if (current_term_ < kReply->term())
            return kReply->term();

          auto &peer = peers_[peer_id];
          std::lock_guard<std::mutex> peer_lock_guard(peer.peer_latch);

          if (kReply->success()) {
            peer.match_index = kReply->update_index();
            peer.next_index = kReply->update_index() + 1;
            replies[peer_id] = kReplyTime + min_sleep_time_
                + std::chrono::round<std::chrono::milliseconds>(dis(gen) * additional_sleep_time_);
          } else {
            LogIndex update_index = kReply->conflict_index();
            if (kReply->conflict_term() != -1) {
              auto last_index_of_term = getLastLogIndexOfTerm(kReply->conflict_term());
              if (last_index_of_term.has_value())
                update_index = last_index_of_term.value();
            }

            peer.match_index = update_index;
            peer.next_index = kReply->update_index() + 1;
            replies[peer_id] = kReplyTime;
          }

          if (std::get<0>(replies[peer_id]) < sleep_until)
            sleep_until = std::get<0>(replies[peer_id]);
        } else {
          auto &peer = peers_[peer_id];
          std::lock_guard<std::mutex> peer_lock_guard(peer.peer_latch);

          protos::raft::AppendEntriesArgs args;
          args.set_term(current_term_);
          args.set_leader_id(me_);
          args.set_prev_log_index(peer.next_index);
          args.set_prev_log_term(getLogEntry(peer.next_index).log_term());
          args.set_leader_commit_index(commit_index_);
          for (LogIndex i = peer.next_index; i < getLastLogEntry().log_index(); i++) {
            args.mutable_entries()->Add();
            *args.mutable_entries(args.entries_size() - 1) = getLogEntry(i);
          }
          replies[peer_id] = appendEntries(peer_id, args);
        }
      }
    }

    for (auto key : reply_keys)
      replies.erase(key);

    std::this_thread::sleep_until(sleep_until);
  }

  return current_term_;
}
std::pair<Raft::LogTerm, bool> Raft::beginLeaderElection() {
  std::vector<Raft::RequestVoteResponse> responses;
  unsigned long peer_count = 0;
  LogTerm term;

  {
    std::lock_guard<std::mutex> lock_guard(latch_);

    term = ++current_term_;
    voted_for_ = me_;
    last_heartbeat_ = std::chrono::system_clock::now();
    peer_count = peers_.size();

    auto &last_log_entry = log_->at(log_->size() - 1);
    auto last_log_index = last_log_entry.log_index();
    auto last_log_term = last_log_entry.log_term();

    auto request_vote_args = protos::raft::RequestVoteArgs{};
    request_vote_args.set_candidate_id(me_);
    request_vote_args.set_term(term);
    request_vote_args.set_last_log_index(last_log_index);
    request_vote_args.set_last_log_term(last_log_term);

    for (auto &peer : peers_) {
      if (peer.first == me_) continue;
      responses.emplace_back(std::move(requestVote(peer.first, request_vote_args)));
    }
  }

  auto vote_count = 1;
  while (!responses.empty() && vote_count <= peer_count / 2) {
    int i = 0;
    while (i < responses.size()) {
      if (responses[i].valid()) {
        auto reply = responses[i].get().first;
        if (reply->vote_granted()) {
          vote_count++;
        } else if (current_term_ < reply->term()) {
          return {reply->term(), false};
        }
      } else {
        i++;
      }
    }

    std::this_thread::sleep_for(min_sleep_time_);
  }

  return {vote_count > peer_count / 2, false};
}

void Raft::updateCommitIndex() {
  auto peer_count = peers_.size();

  std::vector<int> match_indexes(peer_count);
  for (auto &peer : peers_) {
    std::lock_guard<std::mutex> peer_lock_guard(peer.second.peer_latch);
    match_indexes.emplace_back(peer.second.match_index);
  }

  int median_index = static_cast<int>(peer_count) / 2 + 1;
  std::nth_element(match_indexes.begin(), match_indexes.begin() + median_index, match_indexes.end());
  commit_index_ = match_indexes[peer_count / 2 + 1];
}
void Raft::commitEntry(LogIndex entry_index) {
  auto entry = getLogEntry(entry_index);
  entry.set_command_valid(true);
  if (callbacks_.contains(entry.log_name()))
    callbacks_[entry.log_name()](entry.data());
}

protos::raft::LogEntry &Raft::getLogEntry(unsigned long long index) {
  return log_->at(index - log_offset_);
}
protos::raft::LogEntry &Raft::getLastLogEntry() {
  return log_->at(log_->size() - 1);
}
std::optional<Raft::LogIndex> Raft::getLastLogIndexOfTerm(Raft::LogTerm term) {
  auto log_entry_iterator = std::lower_bound(
      log_->begin(),
      log_->end(),
      term,
      [](protos::raft::LogEntry &entry, Raft::LogTerm term) {
        return entry.log_term() < term;
      });
  if (log_entry_iterator == log_->end())
    return std::nullopt;

  auto index = log_entry_iterator->log_index();
  auto last_log_index = getLastLogEntry().log_index();
  while (index < last_log_index && getLogEntry(index + 1).log_term() == term)
    index++;

  return index;
}

}