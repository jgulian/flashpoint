#ifndef FLASHPOINT_CMD_HPP
#define FLASHPOINT_CMD_HPP

#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>

#include "keyvalue/keyvalue.hpp"
#include "keyvalue/plugins/grpc.hpp"
#include "keyvalue/storage/simple_storage.hpp"

namespace flashpoint::cmd {

struct DataOrFileArgs {
  std::string data;
  std::string file;
};

struct ServerConfigArgs {
  std::string snapshot_file;

  bool use_raft = true;
  std::string peer_server_address = "0.0.0.0:3309";
};

struct GetCommandArgs {
  std::string host_address = "localhost:3308";
  DataOrFileArgs key;
};

struct PutCommandArgs {
  std::string host_address = "localhost:3308";
  DataOrFileArgs key;
  DataOrFileArgs value;
};

struct StartCommandArgs {
  std::string host_address = "localhost:3308";
  ServerConfigArgs server_config;
};

struct ConnectCommandArgs {
  std::string host_address = "localhost:3308";
  std::string peer_address = {};
  ServerConfigArgs server_config;
};

std::pair<CLI::App *, GetCommandArgs> setupGetSubcommand(CLI::App &app);

std::pair<CLI::App *, PutCommandArgs> setupPutSubcommand(CLI::App &app);

std::pair<CLI::App *, StartCommandArgs> setupStartSubcommand(CLI::App &app);

std::pair<CLI::App *, ConnectCommandArgs> setupConnectSubcommand(CLI::App &app);


void getCmd(const GetCommandArgs &args);

void putCmd(const PutCommandArgs &args);

void startCmd(const StartCommandArgs &args);

void connectCmd(const ConnectCommandArgs &args);

}// namespace flashpoint::cmd

#endif//FLASHPOINT_CMD_HPP
