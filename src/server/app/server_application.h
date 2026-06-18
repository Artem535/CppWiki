#ifndef CPPWIKI_SRC_SERVER_SERVER_APPLICATION_H_
#define CPPWIKI_SRC_SERVER_SERVER_APPLICATION_H_

#include "server/config/server_config.h"

namespace cppwiki::server {

class ServerApplication final {
 public:
  explicit ServerApplication(ServerConfig config);

  [[nodiscard]] auto Run() const -> int;

 private:
  ServerConfig config_;
};

}  // namespace cppwiki::server

#endif  // CPPWIKI_SRC_SERVER_SERVER_APPLICATION_H_
