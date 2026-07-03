#ifndef CPPWIKI_SRC_SERVER_CONFIG_STATIC_CONFIG_FACTORY_H_
#define CPPWIKI_SRC_SERVER_CONFIG_STATIC_CONFIG_FACTORY_H_

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cppwiki::server::config {

struct ServerAuthConfig final {
  std::optional<std::string> issuer;
  std::optional<std::string> audience;
  std::optional<std::string> jwks_url;
};

struct ServerSyncConfig final {
  bool enabled{false};
  std::optional<std::string> gateway_url;
  std::optional<std::string> database_name;
  std::optional<std::string> admin_url;
  std::map<std::string, std::vector<std::string>> role_channels;
  std::map<std::string, std::vector<std::string>> group_channels;
};

[[nodiscard]] auto MakeStaticConfigYaml(const std::string& host, std::uint16_t port,
                                        const std::string& log_level,
                                        const ServerAuthConfig& auth_config,
                                        const ServerSyncConfig& sync_config,
                                        bool swagger_enabled) -> std::string;

}  // namespace cppwiki::server::config

#endif  // CPPWIKI_SRC_SERVER_CONFIG_STATIC_CONFIG_FACTORY_H_
