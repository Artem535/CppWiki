#ifndef CPPWIKI_SRC_SERVER_DTO_PRESENCE_RESPONSE_H_
#define CPPWIKI_SRC_SERVER_DTO_PRESENCE_RESPONSE_H_

#include <rfl/Rename.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "server/service/presence_service.h"

namespace cppwiki::server::dto {

struct PresenceHeartbeatRequestDto final {
  rfl::Rename<"userId", std::optional<std::string>> user_id{std::nullopt};
  std::optional<std::string> scope;
};

struct PresenceUserDto final {
  rfl::Rename<"userId", std::string> user_id;
  std::string scope;
  rfl::Rename<"lastSeenMsAgo", std::int64_t> last_seen_ms_ago;
};

struct PresenceResultDto final {
  rfl::Rename<"workspaceId", std::string> workspace_id;
  std::vector<PresenceUserDto> users;
};

auto MakePresenceResult(const std::string& workspace_id,
                        const std::vector<service::PresenceInfo>& entries) -> PresenceResultDto;

}  // namespace cppwiki::server::dto

#endif  // CPPWIKI_SRC_SERVER_DTO_PRESENCE_RESPONSE_H_
