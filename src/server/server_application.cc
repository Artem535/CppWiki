#include "server/server_application.h"

#include <drogon/drogon.h>

#include "server/logging/logger.h"

namespace cppwiki::server {

ServerApplication::ServerApplication(ServerConfig config)
    : config_(std::move(config)) {}

auto ServerApplication::Run() -> int {
  logging::InitializeLogging();
  auto logger = logging::ServerLogger();
  logger->info("CppWiki server starting on {}:{}", config_.http_host,
               config_.http_port);

  drogon::app().addListener(config_.http_host, config_.http_port);
  drogon::app().setThreadNum(static_cast<size_t>(config_.thread_num));

  // Controllers and filters are auto-registered by Drogon through reflection.
  drogon::app().run();

  logger->info("CppWiki server stopped");
  return 0;
}

}  // namespace cppwiki::server
