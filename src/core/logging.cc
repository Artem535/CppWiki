#include "core/logging.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

#include <spdlog/spdlog.h>

namespace cppwiki::logging {

namespace {

[[nodiscard]] auto NormalizeLevelName(std::string_view level_name) -> std::string {
  std::string normalized(level_name);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](const unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return normalized;
}

[[nodiscard]] auto ParseLogLevel(std::string_view level_name) -> spdlog::level::level_enum {
  const auto normalized = NormalizeLevelName(level_name);
  if (normalized == "trace") {
    return spdlog::level::trace;
  }
  if (normalized == "debug") {
    return spdlog::level::debug;
  }
  if (normalized == "info") {
    return spdlog::level::info;
  }
  if (normalized == "warn" || normalized == "warning") {
    return spdlog::level::warn;
  }
  if (normalized == "error") {
    return spdlog::level::err;
  }
  if (normalized == "critical") {
    return spdlog::level::critical;
  }
  if (normalized == "off") {
    return spdlog::level::off;
  }

  throw std::invalid_argument("Unsupported log level: " + std::string(level_name));
}

}  // namespace

auto ConfigureBaseLogging() -> void {
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
}

auto ConfigureLogLevel(std::string_view level_name) -> void {
  spdlog::set_level(ParseLogLevel(level_name));
}

}  // namespace cppwiki::logging
