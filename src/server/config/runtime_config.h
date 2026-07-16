#ifndef CPPWIKI_SRC_SERVER_CONFIG_RUNTIME_CONFIG_H_
#define CPPWIKI_SRC_SERVER_CONFIG_RUNTIME_CONFIG_H_

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

namespace cppwiki::server::config {

struct RuntimeConfig final {
  std::string config_path;
  std::optional<std::string> bind_host;
  std::optional<std::uint16_t> port;
  std::optional<std::string> log_level;
  std::optional<std::string> auth_issuer;
  std::optional<std::string> auth_audience;
  std::optional<std::string> auth_jwks_url;
  bool sync_enabled{false};
  std::optional<std::string> sync_gateway_url;
  std::optional<std::string> sync_database_name;
  std::optional<std::string> sync_admin_url;
  std::map<std::string, std::vector<std::string>> sync_role_channels;
  std::map<std::string, std::vector<std::string>> sync_group_channels;
  bool ai_enabled{false};
  std::optional<std::string> ai_base_url;
  std::optional<std::string> ai_api_key;
  std::optional<std::string> ai_model;
  bool swagger{false};

  [[nodiscard]] static auto FromDefaults() -> RuntimeConfig;
  [[nodiscard]] static auto FromCli(int argc, char* argv[]) -> RuntimeConfig;
  [[nodiscard]] auto ToStaticConfigYaml() const -> std::string;

  [[nodiscard]] auto Host() const -> const std::string&;
  [[nodiscard]] auto Port() const -> std::uint16_t;
  [[nodiscard]] auto LogLevel() const -> const std::string&;
};

}  // namespace cppwiki::server::config

#endif  // CPPWIKI_SRC_SERVER_CONFIG_RUNTIME_CONFIG_H_
