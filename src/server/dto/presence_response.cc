#include "server/dto/presence_response.h"

#include <chrono>
#include <utility>

namespace cppwiki::server::dto {

auto MakePresenceResult(const std::string& workspace_id,
                        const std::vector<service::PresenceInfo>& entries) -> PresenceResultDto {
  std::vector<PresenceUserDto> users;
  users.reserve(entries.size());
  for (const auto& entry : entries) {
    users.push_back(PresenceUserDto{
        .user_id = entry.user_id,
        .scope = entry.scope,
        .last_seen_ms_ago =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - entry.last_seen)
                .count(),
    });
  }

  return PresenceResultDto{
      .workspace_id = workspace_id,
      .users = std::move(users),
  };
}

}  // namespace cppwiki::server::dto
