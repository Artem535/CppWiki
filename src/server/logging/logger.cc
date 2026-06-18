#include "server/logging/logger.h"

#include <mutex>

#include <spdlog/sinks/stdout_color_sinks.h>

namespace cppwiki::server::logging {

namespace {

constexpr std::string_view kServerLoggerName = "cppwiki_server";

std::once_flag g_init_flag;
std::shared_ptr<spdlog::logger> g_server_logger;

void InitializeOnce() {
  auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  g_server_logger = std::make_shared<spdlog::logger>(
      std::string{kServerLoggerName}, std::move(sink));
  g_server_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
  g_server_logger->set_level(spdlog::level::info);
}

}  // namespace

void InitializeLogging() {
  std::call_once(g_init_flag, InitializeOnce);
}

auto ServerLogger() -> std::shared_ptr<spdlog::logger> {
  InitializeLogging();
  return g_server_logger;
}

}  // namespace cppwiki::server::logging
