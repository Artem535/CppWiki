#ifndef CPPWIKI_SRC_SERVER_DTO_LOCK_RESPONSE_H_
#define CPPWIKI_SRC_SERVER_DTO_LOCK_RESPONSE_H_

#include <rfl/Rename.hpp>

#include <optional>
#include <string>

namespace cppwiki::server::dto {

struct LockActionResult final {
  bool acquired = false;
  bool released = false;
  bool heartbeat = false;
  bool force_released = false;
  std::optional<std::string> owner;
  std::optional<std::string> document_id;
};

struct LockRequestDto final {
  std::optional<std::string> owner;
};

struct LockResultDto final {
  bool acquired = false;
  bool released = false;
  bool heartbeat = false;
  rfl::Rename<"forceReleased", bool> force_released{false};
  std::optional<std::string> owner;
  rfl::Rename<"documentId", std::optional<std::string>> document_id{std::nullopt};
};

struct LockOwnerResultDto final {
  rfl::Rename<"documentId", std::string> document_id;
  bool locked = false;
  std::optional<std::string> owner;
};

auto MakeLockResult(const LockActionResult& result) -> LockResultDto;
auto MakeLockOwnerResult(const std::string& document_id,
                         const std::optional<std::string>& owner) -> LockOwnerResultDto;

}  // namespace cppwiki::server::dto

#endif  // CPPWIKI_SRC_SERVER_DTO_LOCK_RESPONSE_H_
