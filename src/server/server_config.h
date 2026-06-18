#ifndef CPPWIKI_SRC_SERVER_SERVER_CONFIG_H_
#define CPPWIKI_SRC_SERVER_SERVER_CONFIG_H_

#include <cstdint>
#include <string>

namespace cppwiki::server {

// Static runtime defaults for the Drogon backend.
// Future iterations may load these from a JSON config file or environment.
struct ServerConfig {
  std::string http_host = "0.0.0.0";
  std::uint16_t http_port = 8080;
  std::int32_t thread_num = 0;  // 0 lets Drogon choose based on hardware.
  std::string log_level = "info";
};

}  // namespace cppwiki::server

#endif  // CPPWIKI_SRC_SERVER_SERVER_CONFIG_H_
