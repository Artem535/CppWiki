#ifndef CPPWIKI_SRC_SERVER_APP_SERVER_APPLICATION_H_
#define CPPWIKI_SRC_SERVER_APP_SERVER_APPLICATION_H_

#include "server/config/runtime_config.h"

namespace cppwiki::server {

[[nodiscard]] auto RunServer(const config::RuntimeConfig& config) -> int;

}  // namespace cppwiki::server

#endif  // CPPWIKI_SRC_SERVER_APP_SERVER_APPLICATION_H_
