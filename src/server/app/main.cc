#include <spdlog/spdlog.h>

#include <CLI/CLI.hpp>
#include <cstdlib>
#include <exception>
#include <string>

#include "server/app/server_application.h"
#include "server/config/runtime_config.h"

auto main(int argc, char* argv[]) -> int {
  try {
    const auto config = cppwiki::server::config::RuntimeConfig::FromCli(argc, argv);
    return cppwiki::server::RunServer(config);
  } catch (const std::exception& exception) {
    spdlog::error("cppwiki-server failed: {}", exception.what());
  } catch (...) {
    spdlog::error("cppwiki-server failed with an unknown exception");
  }

  return EXIT_FAILURE;
}
