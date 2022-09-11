#include <CLI/App.hpp>

#include "cmd/cmd.hpp"
#include "util/env.hpp"
#include "util/logger.hpp"


using namespace flashpoint;

void SetupGlobals() {
  util::LoadEnvironmentVariables();
  util::SetLogger(std::make_shared<util::SimpleLogger>());
  util::GetLogger()->Worker();
}

int main(int argc, char **argv) {
  SetupGlobals();

  CLI::App app{"Flashpoint key/value db"};

  cmd::GetCommandArgs get_args{};
  cmd::PutCommandArgs put_args{};
  cmd::CasCommandArgs cas_args{};
  cmd::StartCommandArgs start_args{};

  CLI::App *get = cmd::SetupGetSubcommand(app, get_args);
  CLI::App *put = cmd::SetupPutSubcommand(app, put_args);
  CLI::App *cas = cmd::SetupCasSubcommand(app, cas_args);
  CLI::App *start = cmd::SetupStartSubcommand(app, start_args);

  try {
	app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
	return app.exit(e);
  }

  if (*get) {
	cmd::GetCmd(*get, get_args);
  } else if (*put) {
	cmd::PutCmd(*put, put_args);
  } else if (*cas) {
	cmd::CasCmd(*cas, cas_args);
  } else if (*start) {
	cmd::StartCmd(*start, start_args);
  } else {
	std::cout << app.help() << std::endl;
  }

  auto logger = std::reinterpret_pointer_cast<util::SimpleLogger>(util::GetLogger());
  logger->Kill();
  return 0;
}