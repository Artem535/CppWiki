#ifndef CPPWIKI_SRC_SERVER_SERVER_CONFIG_DTO_H_
#define CPPWIKI_SRC_SERVER_SERVER_CONFIG_DTO_H_

#include <cstdint>
#include <optional>
#include <string>

namespace cppwiki::server {

struct ServerConfigDto final {
  std::optional<std::string> bind_host;
  std::optional<std::uint16_t> port;
  std::optional<bool> enable_swagger;
  std::optional<std::string> log_level;
};

}  // namespace cppwiki::server

#endif  // CPPWIKI_SRC_SERVER_SERVER_CONFIG_DTO_H_
