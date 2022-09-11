#ifndef FLASHPOINT_CMD_HPP
#define FLASHPOINT_CMD_HPP

#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>

#include <grpcpp/server_builder.h>

#include "keyvalue/keyvalue.hpp"

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

struct CasCommandArgs {
  std::string host_address = "localhost:3308";
  DataOrFileArgs key;
  DataOrFileArgs expected;
  DataOrFileArgs updated;
};

struct StartCommandArgs {
  std::string config_file;
  std::string snapshot_file;
};

CLI::App *SetupGetSubcommand(CLI::App &app, GetCommandArgs &command_args);
CLI::App *SetupPutSubcommand(CLI::App &app, PutCommandArgs &command_args);
CLI::App *SetupCasSubcommand(CLI::App &app, CasCommandArgs &command_args);
CLI::App *SetupStartSubcommand(CLI::App &app, StartCommandArgs &command_args);

void GetCmd(CLI::App &get, const GetCommandArgs &command_args);
void PutCmd(CLI::App &put, const PutCommandArgs &command_args);
void CasCmd(CLI::App &cas, const CasCommandArgs &command_args);
void StartCmd(CLI::App &start, const StartCommandArgs &command_args);

}// namespace flashpoint::cmd

#endif//FLASHPOINT_CMD_HPP
