#include <spdlog/spdlog.h>

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

#include "core/constants.h"
#include "server/config/runtime_config.h"

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto TestDefaultRuntimeConfig() -> void {
  const auto config = cppwiki::server::config::RuntimeConfig::FromDefaults();
  Require(config.Host() == cppwiki::constants::kDefaultServerBindHost,
          "default bind host should come from constants");
  Require(config.Port() == cppwiki::constants::kDefaultServerPort,
          "default port should come from constants");
  Require(config.LogLevel() == cppwiki::constants::kDefaultServerLogLevel,
          "default log level should come from constants");
}

auto TestRuntimeConfigFromYaml() -> void {
  const auto config_path =
      std::filesystem::temp_directory_path() / "cppwiki_runtime_config_test.yaml";
  {
    std::ofstream output(config_path);
    Require(output.good(), "temporary yaml config file should be writable");
    output << R"(bind_host: "0.0.0.0"
port: 9091
log_level: WARN # reflect-cpp should ignore this comment and we normalize the value
)";
  }

  std::string config_arg = config_path.string();
  char app_name[] = "cppwiki-server";
  char config_flag[] = "--config";
  char* argv[] = {app_name, config_flag, config_arg.data()};

  const auto cfg = cppwiki::server::config::RuntimeConfig::FromCli(3, argv);
  Require(cfg.Host() == "0.0.0.0", "yaml bind host should be applied");
  Require(cfg.Port() == 9091, "yaml port should be applied");
  Require(cfg.LogLevel() == "warn", "yaml log level should be applied");

  std::filesystem::remove(config_path);
}

auto TestRuntimeConfigOverrides() -> void {
  cppwiki::server::config::RuntimeConfig cfg =
      cppwiki::server::config::RuntimeConfig::FromDefaults();
  cfg.bind_host = "0.0.0.0";
  cfg.port = 9090;
  cfg.log_level = "debug";

  Require(cfg.Host() == "0.0.0.0", "bind host override should be applied");
  Require(cfg.Port() == 9090, "port override should be applied");
  Require(cfg.LogLevel() == "debug", "log level override should be applied");
}

auto TestStaticConfigGeneration() -> void {
  cppwiki::server::config::RuntimeConfig cfg =
      cppwiki::server::config::RuntimeConfig::FromDefaults();
  cfg.bind_host = "127.0.0.1";
  cfg.port = 8081;
  cfg.log_level = "info";

  const auto yaml = cfg.ToStaticConfigYaml();
  Require(!yaml.empty(), "static config YAML must not be empty");
  Require(yaml.find("components_manager:") != std::string::npos,
          "static config must contain components_manager");
  Require(yaml.find("listener:") != std::string::npos, "static config must contain listener");
  Require(yaml.find("port: 8081") != std::string::npos,
          "static config must contain configured port");
  Require(yaml.find("\n        default\n") == std::string::npos,
          "static config must not contain malformed logger entries");
}

}  // namespace

auto main() -> int {
  TestDefaultRuntimeConfig();
  TestRuntimeConfigFromYaml();
  TestRuntimeConfigOverrides();
  TestStaticConfigGeneration();

  spdlog::info("cppwiki_server_config_tests passed");
  return EXIT_SUCCESS;
}
