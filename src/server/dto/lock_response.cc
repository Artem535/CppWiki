#include "server/dto/lock_response.h"

namespace cppwiki::server::dto {

auto MakeLockResult(const LockActionResult& result) -> LockResultDto {
  return LockResultDto{
      .acquired = result.acquired,
      .released = result.released,
      .heartbeat = result.heartbeat,
      .force_released = result.force_released,
      .owner = result.owner,
      .document_id = result.document_id,
  };
}

auto MakeLockOwnerResult(const std::string& document_id,
                         const std::optional<std::string>& owner) -> LockOwnerResultDto {
  return LockOwnerResultDto{
      .document_id = document_id,
      .locked = owner.has_value(),
      .owner = owner,
  };
}

}  // namespace cppwiki::server::dto
