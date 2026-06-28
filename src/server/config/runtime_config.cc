#include "server/config/runtime_config.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <optional>
#include <rfl/yaml/read.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "core/constants.h"
#include "server/config/static_config_factory.h"

namespace cppwiki::server::config {

namespace {

struct AuthConfigFile final {
  std::optional<std::string> issuer;
  std::optional<std::string> audience;
  std::optional<std::string> jwks_url;
};

struct RuntimeConfigFile final {
  struct SyncConfigFile final {
    std::optional<bool> enabled;
    std::optional<std::string> gateway_url;
    std::optional<std::string> database_name;
    std::optional<std::map<std::string, std::vector<std::string>>> role_channels;
    std::optional<std::map<std::string, std::vector<std::string>>> group_channels;
  };

  std::optional<std::string> bind_host;
  std::optional<std::uint16_t> port;
  std::optional<std::string> log_level;
  std::optional<AuthConfigFile> auth;
  std::optional<SyncConfigFile> sync;
};

auto Normalize(std::string_view value) -> std::string {
  std::string out(value);
  std::ranges::transform(out, out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

auto ReadFile(const std::string& path) -> std::string {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Could not open server config: " + path);
  }
  std::ostringstream ss;
  ss << input.rdbuf();
  return ss.str();
}

auto ConvertToUint16(const std::string& value) -> std::uint16_t {
  const long parsed = std::strtol(value.c_str(), nullptr, 10);
  if (parsed <= 0 || parsed > 65535) {
    throw std::invalid_argument("Invalid port value: " + value);
  }
  return static_cast<std::uint16_t>(parsed);
}

auto ConvertLevel(const std::string& level) -> std::string {
  if (const auto normalized = Normalize(level); normalized == "trace" || normalized == "debug" ||
                                                normalized == "info" || normalized == "warn" ||
                                                normalized == "error" || normalized == "critical" ||
                                                normalized == "off" || normalized == "warning") {
    return normalized;
  }
  throw std::invalid_argument("Unsupported log level: " + level);
}

auto LoadRuntimeConfigFile(const std::string& path) -> RuntimeConfigFile {
  const auto yaml = ReadFile(path);
  auto parsed = rfl::yaml::read<RuntimeConfigFile>(yaml);
  if (!parsed) {
    throw std::runtime_error("Could not parse server config: " + path + ": " +
                             parsed.error().what());
  }

  auto file_config = parsed.value();
  if (file_config.port) {
    file_config.port = ConvertToUint16(std::to_string(*file_config.port));
  }
  if (file_config.log_level) {
    file_config.log_level = ConvertLevel(*file_config.log_level);
  }

  return file_config;
}

}  // namespace

auto RuntimeConfig::FromDefaults() -> RuntimeConfig {
  return RuntimeConfig{
      .config_path = std::string(cppwiki::constants::kDefaultServerConfigPath),
      .bind_host = std::nullopt,
      .port = std::nullopt,
      .log_level = std::nullopt,
      .auth_issuer = std::nullopt,
      .auth_audience = std::nullopt,
      .auth_jwks_url = std::nullopt,
      .sync_enabled = false,
      .sync_gateway_url = std::nullopt,
      .sync_database_name = std::nullopt,
      .sync_role_channels = {},
      .sync_group_channels = {},
      .swagger = false,
  };
}

auto RuntimeConfig::FromCli(int argc, char* argv[]) -> RuntimeConfig {
  RuntimeConfig cfg = FromDefaults();
  std::optional<std::string> cli_bind_host;
  std::optional<int> parsed_port;
  std::optional<std::string> cli_log_level;

  CLI::App app("CppWiki userver server");
  app.add_option("-c,--config", cfg.config_path, "Path to YAML server config file");
  app.add_option("--bind-host", cli_bind_host, "Bind host override");

  auto* port_option = app.add_option("--port", parsed_port, "Port override");
  port_option->check(CLI::Range(1, 65535));

  app.add_option("--log-level", cli_log_level,
                 "Log level override: trace, debug, info, warn, error, critical, off");

  bool swagger_enabled = false;
  bool swagger_disabled = false;
  app.add_flag("--swagger", swagger_enabled, "Enable Swagger UI (placeholder)");
  app.add_flag("--no-swagger", swagger_disabled, "Disable Swagger UI");

  app.parse(argc, argv);

  if (!cfg.config_path.empty() && cfg.config_path != "-") {
    const auto file_config = LoadRuntimeConfigFile(cfg.config_path);
    cfg.bind_host = file_config.bind_host;
    cfg.port = file_config.port;
    cfg.log_level = file_config.log_level;
    if (file_config.auth) {
      cfg.auth_issuer = file_config.auth->issuer;
      cfg.auth_audience = file_config.auth->audience;
      cfg.auth_jwks_url = file_config.auth->jwks_url;
    }
    if (file_config.sync) {
      cfg.sync_enabled = file_config.sync->enabled.value_or(false);
      cfg.sync_gateway_url = file_config.sync->gateway_url;
      cfg.sync_database_name = file_config.sync->database_name;
      cfg.sync_role_channels = file_config.sync->role_channels.value_or(decltype(cfg.sync_role_channels){});
      cfg.sync_group_channels = file_config.sync->group_channels.value_or(decltype(cfg.sync_group_channels){});
    }
  }

  if (cli_bind_host) {
    cfg.bind_host = std::move(cli_bind_host);
  }

  if (parsed_port) {
    cfg.port = static_cast<std::uint16_t>(*parsed_port);
  }

  if (cli_log_level) {
    cfg.log_level = ConvertLevel(*cli_log_level);
  }

  if (swagger_enabled && swagger_disabled) {
    throw std::runtime_error("Use only one of --swagger or --no-swagger.");
  }
  if (swagger_enabled) {
    cfg.swagger = true;
  } else if (swagger_disabled) {
    cfg.swagger = false;
  }

  return cfg;
}

auto RuntimeConfig::ToStaticConfigYaml() const -> std::string {
  return MakeStaticConfigYaml(
      Host(), Port(), LogLevel(),
      ServerAuthConfig{
          .issuer = auth_issuer,
          .audience = auth_audience,
          .jwks_url = auth_jwks_url,
      },
      ServerSyncConfig{
          .enabled = sync_enabled,
          .gateway_url = sync_gateway_url,
          .database_name = sync_database_name,
          .role_channels = sync_role_channels,
          .group_channels = sync_group_channels,
      },
      swagger);
}

auto RuntimeConfig::Host() const -> const std::string& {
  if (bind_host) {
    return *bind_host;
  }

  static const std::string kDefaultHost(cppwiki::constants::kDefaultServerBindHost);
  return kDefaultHost;
}

auto RuntimeConfig::Port() const -> std::uint16_t {
  if (port) {
    return *port;
  }

  return cppwiki::constants::kDefaultServerPort;
}

auto RuntimeConfig::LogLevel() const -> const std::string& {
  if (log_level) {
    return *log_level;
  }

  static const std::string kDefaultLevel(cppwiki::constants::kDefaultServerLogLevel);
  return kDefaultLevel;
}

}  // namespace cppwiki::server::config
