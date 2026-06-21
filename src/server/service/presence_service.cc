#include "server/service/presence_service.h"

#include <string>
#include <vector>

namespace cppwiki::server::service {

auto PresenceService::Heartbeat(const std::string& workspace_id, const std::string& user_id,
                                const std::string& scope) -> void {
  std::lock_guard lock(mutex_);
  presence_by_workspace_[workspace_id][user_id] = PresenceInfo{
      .user_id = user_id,
      .scope = scope,
      .last_seen = std::chrono::steady_clock::now(),
  };
}

auto PresenceService::GetPresence(const std::string& workspace_id) const -> std::vector<PresenceInfo> {
  std::lock_guard lock(mutex_);
  std::vector<PresenceInfo> result;
  const auto it = presence_by_workspace_.find(workspace_id);
  if (it == presence_by_workspace_.end()) {
    return result;
  }

  result.reserve(it->second.size());
  for (const auto& [_, info] : it->second) {
    result.push_back(info);
  }
  return result;
}

}  // namespace cppwiki::server::service
