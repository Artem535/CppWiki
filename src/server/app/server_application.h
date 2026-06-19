#ifndef CPPWIKI_SRC_SERVER_APP_SERVER_APPLICATION_H_
#define CPPWIKI_SRC_SERVER_APP_SERVER_APPLICATION_H_

#include "server/config/runtime_config.h"

namespace cppwiki::server {

class ServerApplication final {
 public:
  explicit ServerApplication(config::RuntimeConfig config);

  [[nodiscard]] auto Run() const -> int;

 private:
  config::RuntimeConfig config_;
};

}  // namespace cppwiki::server

#endif  // CPPWIKI_SRC_SERVER_APP_SERVER_APPLICATION_H_
