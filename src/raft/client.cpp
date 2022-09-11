#include "raft/client.hpp"

namespace flashpoint::raft {
RaftClient::RaftClient(PeerId me, const protos::raft::Config &config) : me_(std::move(me)) { config_.CopyFrom(config); }

void RaftClient::UpdateConfig(const protos::raft::Config &config) {
  std::unique_lock<std::shared_mutex> write_lock(*lock_);
  config_.CopyFrom(config);
  cached_connection_info_ = std::nullopt;
}

LogIndex RaftClient::Start(const std::string &command) {
  protos::raft::StartRequest request = {};
  request.mutable_log_data()->set_data(command);
  request.mutable_log_data()->set_type(protos::raft::COMMAND);
  return DoRequest(request);
}
LogIndex RaftClient::StartConfig(const protos::raft::Config &config) {
  protos::raft::StartRequest request = {};
  request.mutable_log_data()->set_data(config.SerializeAsString());
  request.mutable_log_data()->set_type(protos::raft::CONFIG);
  return DoRequest(request);
}

LogIndex RaftClient::DoRequest(const protos::raft::StartRequest &request) {
  std::shared_lock<std::shared_mutex> lock(*lock_);

  CheckForCacheExistence(lock);
  auto &[leader_id, stub] = cached_connection_info_.value();

  auto time = std::chrono::system_clock::now();
  std::chrono::time_point<std::chrono::system_clock, std::chrono::system_clock::duration> last_call;
  protos::raft::StartResponse response = {};
  grpc::Status status = {};
  do {
	grpc::ClientContext client_context;
	last_call = std::chrono::system_clock::now();
	auto deadline = last_call + kStartRequestBuffer;
	client_context.set_deadline(deadline);

	status = stub->Start(&client_context, request, &response);

	if (!status.ok())
	  UpdateCache(lock, me_);
	else if (status.ok() && response.data_case() == protos::raft::StartResponse::DataCase::kLeaderId)
	  UpdateCache(lock, response.leader_id());

	if (std::chrono::system_clock::now() - time > kStartRequestTimeout) {
	  throw std::runtime_error("Start request timeout");
	}

	if (std::chrono::system_clock::now() < deadline)
	  std::this_thread::sleep_until(deadline);
  } while (!status.ok() || response.data_case() != protos::raft::StartResponse::DataCase::kLogIndex);

  return response.log_index();
}
void RaftClient::CheckForCacheExistence(std::shared_lock<std::shared_mutex> &lock) {
  if (cached_connection_info_.has_value())
	return;

  lock.unlock();
  {
	std::unique_lock<std::shared_mutex> write_lock(*lock_);
	if (cached_connection_info_.has_value()) {
	  write_lock.unlock();
	  lock.lock();
	  return;
	}

	auto &kPeer = config_.peers(0);
	auto channel = grpc::CreateChannel(kPeer.data().address(), grpc::InsecureChannelCredentials());
	auto stub = protos::raft::Raft::NewStub(channel);
	cached_connection_info_ = {kPeer.id(), std::move(stub)};
  }

  lock.lock();
}
void RaftClient::UpdateCache(std::shared_lock<std::shared_mutex> &lock, const std::string &leader_id) {
  lock.unlock();

  {
	std::unique_lock<std::shared_mutex> write_lock(*lock_);

	if (cached_connection_info_->first == leader_id) {
	  write_lock.unlock();
	  lock.lock();
	  return;
	}

	for (auto &kPeer : config_.peers()) {
	  if (kPeer.id() == leader_id) {
		auto channel = grpc::CreateChannel(kPeer.data().address(), grpc::InsecureChannelCredentials());
		auto stub = protos::raft::Raft::NewStub(channel);
		cached_connection_info_ = {kPeer.id(), std::move(stub)};
		write_lock.unlock();
		lock.lock();
		return;
	  }
	}
  }

  lock.lock();
  throw std::runtime_error("no such peer");
}
}