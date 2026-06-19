#ifndef CPPWIKI_SRC_SERVER_DTO_LOCK_RESPONSE_H_
#define CPPWIKI_SRC_SERVER_DTO_LOCK_RESPONSE_H_

#include <optional>
#include <string>

#include <userver/formats/json/value.hpp>

namespace cppwiki::server::dto {

struct LockActionResult final {
  bool acquired = false;
  bool released = false;
  bool heartbeat = false;
  bool force_released = false;
  std::optional<std::string> owner;
  std::optional<std::string> document_id;
};

auto MakeLockResultJson(const LockActionResult& result) -> userver::formats::json::Value;
auto MakeLockOwnerResultJson(const std::string& document_id,
                             const std::optional<std::string>& owner) -> userver::formats::json::Value;

}  // namespace cppwiki::server::dto

#endif  // CPPWIKI_SRC_SERVER_DTO_LOCK_RESPONSE_H_
