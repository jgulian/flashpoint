#include <CLI/App.hpp>

#include "cmd/cmd.hpp"
#include "util/env.hpp"
#include "util/logger.hpp"


using namespace flashpoint;

void setup_globals() {
  util::LoadEnvironmentVariables();
  util::LOGGER = std::make_unique<util::ManualLogger>();
}

int main(int argc, char **argv) {
  setup_globals();

  CLI::App app{"Flashpoint key/value db"};

  cmd::GetCommandArgs get_args;
  cmd::PutCommandArgs put_args;
  cmd::StartCommandArgs start_args;

  CLI::App *get = cmd::setupGetSubcommand(app, get_args);
  CLI::App *put = cmd::setupPutSubcommand(app, put_args);
  CLI::App *start = cmd::setupStartSubcommand(app, start_args);

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
  } else {
    std::cout << app.help() << std::endl;
  }

  return 0;
}