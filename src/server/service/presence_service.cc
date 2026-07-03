#include "server/service/presence_service.h"

#include <string>
#include <vector>

namespace cppwiki::server::service {

auto PresenceService::PruneExpired(WorkspacePresenceMap& workspace_presence,
                                   std::chrono::steady_clock::time_point now) -> void {
  std::erase_if(workspace_presence, [now](const auto& item) {
    return now - item.second.last_seen > kPresenceTtl;
  });
}

auto PresenceService::Heartbeat(const std::string& workspace_id, const std::string& user_id,
                                const std::string& scope) -> void {
  std::lock_guard lock(mutex_);
  const auto now = std::chrono::steady_clock::now();
  auto& workspace_presence = presence_by_workspace_[workspace_id];
  PruneExpired(workspace_presence, now);
  workspace_presence[user_id] = PresenceInfo{
      .user_id = user_id,
      .scope = scope,
      .last_seen = now,
  };
}

auto PresenceService::GetPresence(const std::string& workspace_id) -> std::vector<PresenceInfo> {
  std::lock_guard lock(mutex_);
  std::vector<PresenceInfo> result;
  const auto it = presence_by_workspace_.find(workspace_id);
  if (it == presence_by_workspace_.end()) {
    return result;
  }

  const auto now = std::chrono::steady_clock::now();
  auto& workspace_presence = it->second;
  PruneExpired(workspace_presence, now);
  if (workspace_presence.empty()) {
    presence_by_workspace_.erase(it);
    return result;
  }

  result.reserve(workspace_presence.size());
  for (const auto& [_, info] : workspace_presence) {
    result.push_back(info);
  }
  return result;
}

}  // namespace cppwiki::server::service
