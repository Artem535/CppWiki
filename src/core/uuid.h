#ifndef CPPWIKI_SRC_CORE_UUID_H_
#define CPPWIKI_SRC_CORE_UUID_H_

#include <string>

namespace cppwiki {

[[nodiscard]] auto GenerateUuidString() -> std::string;

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_CORE_UUID_H_
