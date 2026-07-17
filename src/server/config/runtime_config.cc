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
#include <vector>

#include "core/constants.h"
#include "server/config/static_config_factory.h"

namespace cppwiki::server::config {

namespace detail {

struct AuthConfigFile final {
  std::optional<std::string> issuer;
  std::optional<std::string> audience;
  std::optional<std::string> jwks_url;
};

struct SyncConfigFile final {
  std::optional<bool> enabled;
  std::optional<std::string> gateway_url;
  std::optional<std::string> database_name;
  std::optional<std::string> admin_url;
  std::optional<std::map<std::string, std::vector<std::string>>> role_channels;
  std::optional<std::map<std::string, std::vector<std::string>>> group_channels;
};

struct AiConfigFile final {
  std::optional<bool> enabled;
  std::optional<std::string> base_url;
  std::optional<std::string> api_key;
  std::optional<std::string> model;
  std::optional<std::uint32_t> timeout_seconds;
};

struct RuntimeConfigFile final {
  std::optional<std::string> bind_host;
  std::optional<std::uint16_t> port;
  std::optional<std::string> log_level;
  std::optional<AuthConfigFile> auth;
  std::optional<SyncConfigFile> sync;
  std::optional<AiConfigFile> ai;
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

auto ReadEnv(std::string_view name) -> std::optional<std::string> {
  const auto* value = std::getenv(std::string(name).c_str());
  if (value == nullptr || std::string_view(value).empty()) {
    return std::nullopt;
  }
  return std::string(value);
}

auto ReadBoolEnv(std::string_view name) -> std::optional<bool> {
  const auto value = ReadEnv(name);
  if (!value) {
    return std::nullopt;
  }

  const auto normalized = Normalize(*value);
  if (normalized == "1" || normalized == "true" || normalized == "yes" ||
      normalized == "on") {
    return true;
  }
  if (normalized == "0" || normalized == "false" || normalized == "no" ||
      normalized == "off") {
    return false;
  }

  throw std::invalid_argument("Unsupported boolean environment value for " + std::string(name) +
                              ": " + *value);
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

void ApplyEnvironmentOverrides(RuntimeConfig& cfg) {
  if (auto value = ReadEnv("CPPWIKI_BIND_HOST")) {
    cfg.bind_host = std::move(value);
  }
  if (auto value = ReadEnv("CPPWIKI_PORT")) {
    cfg.port = ConvertToUint16(*value);
  }
  if (auto value = ReadEnv("CPPWIKI_LOG_LEVEL")) {
    cfg.log_level = ConvertLevel(*value);
  }

  if (auto value = ReadEnv("CPPWIKI_AUTH_ISSUER")) {
    cfg.auth_issuer = std::move(value);
  }
  if (auto value = ReadEnv("CPPWIKI_AUTH_AUDIENCE")) {
    cfg.auth_audience = std::move(value);
  }
  if (auto value = ReadEnv("CPPWIKI_AUTH_JWKS_URL")) {
    cfg.auth_jwks_url = std::move(value);
  }

  if (auto value = ReadBoolEnv("CPPWIKI_SYNC_ENABLED")) {
    cfg.sync_enabled = *value;
  }
  if (auto value = ReadEnv("CPPWIKI_SYNC_GATEWAY_URL")) {
    cfg.sync_gateway_url = std::move(value);
  }
  if (auto value = ReadEnv("CPPWIKI_SYNC_ADMIN_URL")) {
    cfg.sync_admin_url = std::move(value);
  }
  if (auto value = ReadEnv("CPPWIKI_SYNC_DATABASE_NAME")) {
    cfg.sync_database_name = std::move(value);
  }

  if (auto value = ReadBoolEnv("CPPWIKI_AI_ENABLED")) {
    cfg.ai_enabled = *value;
  }
  if (auto value = ReadEnv("CPPWIKI_AI_BASE_URL")) {
    cfg.ai_base_url = std::move(value);
  }
  if (auto value = ReadEnv("CPPWIKI_AI_API_KEY")) {
    cfg.ai_api_key = std::move(value);
  }
  if (auto value = ReadEnv("CPPWIKI_AI_MODEL")) {
    cfg.ai_model = std::move(value);
  }
  if (auto value = ReadEnv("CPPWIKI_AI_TIMEOUT_SECONDS")) {
    cfg.ai_timeout_seconds = static_cast<std::uint32_t>(std::strtoul(value->c_str(), nullptr, 10));
  }
}

}  // namespace detail

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
      .sync_admin_url = std::nullopt,
      .sync_role_channels = {},
      .sync_group_channels = {},
      .ai_enabled = false,
      .ai_base_url = std::nullopt,
      .ai_api_key = std::nullopt,
      .ai_model = std::nullopt,
      .ai_timeout_seconds = std::nullopt,
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
    const auto file_config = detail::LoadRuntimeConfigFile(cfg.config_path);
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
      cfg.sync_admin_url = file_config.sync->admin_url;
      cfg.sync_role_channels = file_config.sync->role_channels.value_or(decltype(cfg.sync_role_channels){});
      cfg.sync_group_channels = file_config.sync->group_channels.value_or(decltype(cfg.sync_group_channels){});
    }
    if (file_config.ai) {
      cfg.ai_enabled = file_config.ai->enabled.value_or(false);
      cfg.ai_base_url = file_config.ai->base_url;
      cfg.ai_api_key = file_config.ai->api_key;
      cfg.ai_model = file_config.ai->model;
      cfg.ai_timeout_seconds = file_config.ai->timeout_seconds;
    }
  }

  if (cli_bind_host) {
    cfg.bind_host = std::move(cli_bind_host);
  }

  if (parsed_port) {
    cfg.port = static_cast<std::uint16_t>(*parsed_port);
  }

  if (cli_log_level) {
    cfg.log_level = detail::ConvertLevel(*cli_log_level);
  }

  if (swagger_enabled && swagger_disabled) {
    throw std::runtime_error("Use only one of --swagger or --no-swagger.");
  }
  if (swagger_enabled) {
    cfg.swagger = true;
  } else if (swagger_disabled) {
    cfg.swagger = false;
  }

  detail::ApplyEnvironmentOverrides(cfg);

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
          .admin_url = sync_admin_url,
          .role_channels = sync_role_channels,
          .group_channels = sync_group_channels,
      },
      ServerAiConfig{
          .enabled = ai_enabled,
          .base_url = ai_base_url,
          .api_key = ai_api_key,
          .model = ai_model,
          .timeout_seconds = ai_timeout_seconds,
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
