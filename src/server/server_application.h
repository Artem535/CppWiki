#ifndef CPPWIKI_SRC_SERVER_SERVER_APPLICATION_H_
#define CPPWIKI_SRC_SERVER_SERVER_APPLICATION_H_

#include "server/server_config.h"

namespace cppwiki::server {

// Entry point for the Drogon backend.
// Configures the HTTP server, registers controllers/filters and starts the
// event loop. This class intentionally has no Qt dependency.
class ServerApplication {
 public:
  explicit ServerApplication(ServerConfig config);
  ServerApplication(const ServerApplication&) = delete;
  auto operator=(const ServerApplication&) -> ServerApplication& = delete;

  // Returns 0 on clean shutdown, non-zero on startup error.
  [[nodiscard]] auto Run() -> int;

 private:
  ServerConfig config_;
};

}  // namespace cppwiki::server

#endif  // CPPWIKI_SRC_SERVER_SERVER_APPLICATION_H_
