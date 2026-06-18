#ifndef CPPWIKI_SRC_SERVER_LOGGING_LOGGER_H_
#define CPPWIKI_SRC_SERVER_LOGGING_LOGGER_H_

#include <memory>
#include <string_view>

#include <spdlog/logger.h>

namespace cppwiki::server::logging {

// Returns a spdlog logger configured for the server component.
// The logger is created lazily and shared.
[[nodiscard]] auto ServerLogger() -> std::shared_ptr<spdlog::logger>;

// Initialize default sinks and logger name for the server.
// Safe to call multiple times.
void InitializeLogging();

}  // namespace cppwiki::server::logging

#endif  // CPPWIKI_SRC_SERVER_LOGGING_LOGGER_H_
