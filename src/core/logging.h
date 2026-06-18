#ifndef CPPWIKI_SRC_CORE_LOGGING_H_
#define CPPWIKI_SRC_CORE_LOGGING_H_

#include <string_view>

namespace cppwiki::logging {

auto ConfigureBaseLogging() -> void;
auto ConfigureLogLevel(std::string_view level_name) -> void;

}  // namespace cppwiki::logging

#endif  // CPPWIKI_SRC_CORE_LOGGING_H_
