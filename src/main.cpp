#include <CLI/App.hpp>

#include "cmd/cmd.hpp"
#include "util/logger.hpp"
#include "util/thread_pool.hpp"

using namespace flashpoint;

void setup_globals() {
  util::LOGGER = std::make_unique<util::SimpleLogger>();
  util::THREAD_POOL = std::make_unique<util::ThreadPool>(4);
}

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

  for (int i = 0; i < argc; i++)
    std::cout << argv[i] << std::endl;

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    return app.exit(e);
  }

  if (*get) {
    cmd::getCmd(*get, get_args);
  } else if (*put) {
    cmd::putCmd(*put, put_args);
  } else if (*start) {
    cmd::startCmd(*start, start_args);
  } else if (*connect) {
    cmd::connectCmd(*connect, connect_args);
  } else {
    std::cout << app.help() << std::endl;
  }

  return 0;
}