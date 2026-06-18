#include <rfl/yaml/read.hpp>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

#include "core/constants.h"
#include "server/config/server_config.h"
#include "server/config/server_config_dto.h"

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto TestDefaultServerConfig() -> void {
  const auto config = cppwiki::server::ServerConfig::FromDefaults();
  Require(config.BindHost() == cppwiki::constants::kDefaultServerBindHost,
          "default bind host should come from constants");
  Require(config.Port() == cppwiki::constants::kDefaultServerPort,
          "default port should come from constants");
  Require(config.SwaggerEnabled() == cppwiki::constants::kDefaultServerSwaggerEnabled,
          "default swagger flag should come from constants");
  Require(config.LogLevel() == cppwiki::constants::kDefaultServerLogLevel,
          "default log level should come from constants");
}

auto TestServerConfigOverrides() -> void {
  const auto config = cppwiki::server::ServerConfig::FromDefaults().WithOverrides(
      std::optional<std::string>("0.0.0.0"), std::optional<std::uint16_t>(9090),
      std::optional<bool>(false), std::optional<std::string>("debug"));
  Require(config.BindHost() == "0.0.0.0", "bind host override should be applied");
  Require(config.Port() == 9090, "port override should be applied");
  Require(!config.SwaggerEnabled(), "swagger override should be applied");
  Require(config.LogLevel() == "debug", "log level override should be applied");
}

auto TestServerConfigFromYamlFile() -> void {
  const auto config_path =
      std::filesystem::temp_directory_path() / "cppwiki_server_config_test.yaml";
  {
    std::ofstream output(config_path);
    Require(output.good(), "temporary yaml config file should be writable");
    output << R"(bind_host: 0.0.0.0
port: 9091
enable_swagger: false
log_level: warn
)";
  }

  const auto config = cppwiki::server::ServerConfig::FromYamlFile(config_path.string());
  Require(config.BindHost() == "0.0.0.0", "yaml bind host should be applied");
  Require(config.Port() == 9091, "yaml port should be applied");
  Require(!config.SwaggerEnabled(), "yaml swagger flag should be applied");
  Require(config.LogLevel() == "warn", "yaml log level should be applied");

  std::filesystem::remove(config_path);
}

auto TestReflectCppConfigYamlParse() -> void {
  const auto parsed = rfl::yaml::read<cppwiki::server::ServerConfigDto>(std::string(R"(bind_host: 127.0.0.1
port: 8080
enable_swagger: true
log_level: info
)"));
  Require(static_cast<bool>(parsed), "reflect-cpp must read YAML config DTO");
  Require(parsed->bind_host == "127.0.0.1", "config DTO parse must preserve bind host");
  Require(parsed->port == 8080, "config DTO parse must preserve port");
  Require(parsed->enable_swagger.value_or(false),
          "config DTO parse must preserve swagger flag");
  Require(parsed->log_level == "info", "config DTO parse must preserve log level");
}

}  // namespace

auto main() -> int {
  TestDefaultServerConfig();
  TestServerConfigOverrides();
  TestServerConfigFromYamlFile();
  TestReflectCppConfigYamlParse();

  spdlog::info("cppwiki_server_config_tests passed");
  return EXIT_SUCCESS;
}
