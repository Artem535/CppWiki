#ifndef CPPWIKI_SRC_SERVER_CONFIG_STATIC_CONFIG_FACTORY_H_
#define CPPWIKI_SRC_SERVER_CONFIG_STATIC_CONFIG_FACTORY_H_

#include <cstdint>
#include <string>

namespace cppwiki::server::config {

[[nodiscard]] auto MakeStaticConfigYaml(const std::string& host, std::uint16_t port,
                                        const std::string& log_level, bool swagger_enabled)
    -> std::string;

}  // namespace cppwiki::server::config

#endif  // CPPWIKI_SRC_SERVER_CONFIG_STATIC_CONFIG_FACTORY_H_
