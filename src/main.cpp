#include <CLI/App.hpp>

#include "cmd/cmd.hpp"

using namespace flashpoint;

int main(int argc, char **argv) {
  CLI::App app{"Flashpoint key/value db"};

  auto [get, get_args] = cmd::setupGetSubcommand(app);
  auto [put, put_args] = cmd::setupPutSubcommand(app);
  auto [start, start_args] = cmd::setupStartSubcommand(app);
  auto [connect, connect_args] = cmd::setupConnectSubcommand(app);

  CLI11_PARSE(app, argc, argv);

  if (*get) {
    cmd::getCmd(get_args);
  } else if (*put) {
    cmd::putCmd(put_args);
  } else if (*start) {
    cmd::startCmd(start_args);
  } else if (*connect) {
    cmd::connectCmd(connect_args);
  } else {
    std::cout << app.help() << std::endl;
  }

  return 0;
}