#ifndef FLASHPOINT_CMD_HPP
#define FLASHPOINT_CMD_HPP

#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>

#include <grpcpp/server_builder.h>

#include "keyvalue/keyvalue.hpp"

#include "cmd/server.hpp"

namespace flashpoint::cmd {

struct DataOrFileArgs {
  std::string data;
  std::string file;
};

struct GetCommandArgs {
  std::string host_address = "localhost:3308";
  DataOrFileArgs key;
  std::string output_file = {};
};

struct PutCommandArgs {
  std::string host_address = "localhost:3308";
  DataOrFileArgs key;
  DataOrFileArgs value;
};

struct StartCommandArgs {
  std::string host_address = "localhost:3308";
  std::string peer_server_address = "0.0.0.0:3309";
  std::string snapshot_file;
};


CLI::App *setupGetSubcommand(CLI::App &app, GetCommandArgs &command_args);

CLI::App *setupPutSubcommand(CLI::App &app, PutCommandArgs &command_args);

CLI::App *setupStartSubcommand(CLI::App &app, StartCommandArgs &command_args);



void getCmd(CLI::App &get, const GetCommandArgs &command_args);

void putCmd(CLI::App &put, const PutCommandArgs &args);

void startCmd(CLI::App &start, const StartCommandArgs &command_args);

}// namespace flashpoint::cmd

#endif//FLASHPOINT_CMD_HPP
