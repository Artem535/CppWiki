#ifndef CPPWIKI_SRC_SERVER_CONFIG_STATIC_CONFIG_FACTORY_H_
#define CPPWIKI_SRC_SERVER_CONFIG_STATIC_CONFIG_FACTORY_H_

#include <cstdint>
#include <optional>
#include <string>

namespace cppwiki::server::config {

struct ServerAuthConfig final {
  std::optional<std::string> issuer;
  std::optional<std::string> audience;
  std::optional<std::string> jwks_url;
};

[[nodiscard]] auto MakeStaticConfigYaml(const std::string& host, std::uint16_t port,
                                        const std::string& log_level,
                                        const ServerAuthConfig& auth_config,
                                        bool swagger_enabled) -> std::string;

}  // namespace cppwiki::server::config

#endif  // CPPWIKI_SRC_SERVER_CONFIG_STATIC_CONFIG_FACTORY_H_
