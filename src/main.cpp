#include <CLI/App.hpp>

#include "cmd/cmd.hpp"

using namespace flashpoint;

int main(int argc, char **argv) {
  CLI::App app{"Flashpoint key/value db"};

  cmd::GetCommandArgs get_args;
  cmd::PutCommandArgs put_args;
  cmd::StartCommandArgs start_args;
  cmd::ConnectCommandArgs connect_args;

  CLI::App *get = cmd::setupGetSubcommand(app, get_args);
  CLI::App *put = cmd::setupPutSubcommand(app, put_args);
  CLI::App *start = cmd::setupStartSubcommand(app, start_args);
  CLI::App *connect = cmd::setupConnectSubcommand(app, connect_args);

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    return app.exit(e);
  }

  std::cout << get_args.host_address << std::endl;

  std::cout << "here" << std::endl;
  if (*get) {
    std::cout << "get" << std::endl;
    cmd::getCmd(*get, get_args);
  } else if (*put) {
    std::cout << "put" << std::endl;
    cmd::putCmd(*put, put_args);
  } else if (*start) {
    std::cout << "start" << std::endl;
    cmd::startCmd(*start, start_args);
  } else if (*connect) {
    std::cout << "connect" << std::endl;
    cmd::connectCmd(*connect, connect_args);
  } else {
    std::cout << app.help() << std::endl;
  }
  std::cout << "here1" << std::endl;

  return 0;
}