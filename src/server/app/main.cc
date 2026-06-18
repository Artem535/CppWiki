#include <spdlog/spdlog.h>

#include <CLI/CLI.hpp>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>

#include "core/constants.h"
#include "core/logging.h"
#include "oatpp/core/base/Environment.hpp"
#include "server/app/server_application.h"
#include "server/config/server_config.h"

namespace {

class OatppEnvironmentGuard final {
 public:
  OatppEnvironmentGuard() {
    oatpp::base::Environment::init();
  }

  ~OatppEnvironmentGuard() {
    oatpp::base::Environment::destroy();
  }
};

struct ServerCliOptions final {
  std::string config_path = std::string(cppwiki::constants::kDefaultServerConfigPath);
  std::optional<std::string> bind_host;
  std::optional<std::uint16_t> port;
  std::optional<bool> enable_swagger;
  std::optional<std::string> log_level;
};

}  // namespace

auto main(int argc, char* argv[]) -> int {
  OatppEnvironmentGuard environment_guard;
  cppwiki::logging::ConfigureBaseLogging();

  try {
    ServerCliOptions cli_options;
    bool swagger_enabled_flag = false;
    bool swagger_disabled_flag = false;
    int port_value = 0;

    CLI::App app("CppWiki oat++ server");
    app.add_option("-c,--config", cli_options.config_path, "Path to YAML server config file");
    app.add_option("--bind-host", cli_options.bind_host, "Bind host override");
    app.add_option("--port", port_value, "Port override")->check(CLI::Range(1, 65535));
    app.add_option("--log-level", cli_options.log_level,
                   "Log level override: trace, debug, info, warn, error, critical, off");
    app.add_flag("--swagger", swagger_enabled_flag, "Enable Swagger UI");
    app.add_flag("--no-swagger", swagger_disabled_flag, "Disable Swagger UI");
    CLI11_PARSE(app, argc, argv);

    if (port_value > 0) {
      cli_options.port = static_cast<std::uint16_t>(port_value);
    }

    if (swagger_enabled_flag && swagger_disabled_flag) {
      throw std::runtime_error("Use only one of --swagger or --no-swagger.");
    }
    if (swagger_enabled_flag) {
      cli_options.enable_swagger = true;
    } else if (swagger_disabled_flag) {
      cli_options.enable_swagger = false;
    }

    const auto server_config =
        cppwiki::server::ServerConfig::FromYamlFile(cli_options.config_path)
            .WithOverrides(cli_options.bind_host, cli_options.port, cli_options.enable_swagger,
                           cli_options.log_level);
    cppwiki::logging::ConfigureLogLevel(server_config.LogLevel());

    const cppwiki::server::ServerApplication application(server_config);
    return application.Run();
  } catch (const std::exception& exception) {
    spdlog::error("cppwiki-server failed: {}", exception.what());
  } catch (...) {
    spdlog::error("cppwiki-server failed with an unknown exception");
  }

  return EXIT_FAILURE;
}
