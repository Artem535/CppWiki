#include "server/service/presence_service.h"

#include <string>
#include <vector>

namespace cppwiki::server::service {

auto PresenceService::Heartbeat(const std::string& workspace_id, const std::string& user_id,
                                const std::string& scope) -> void {
  const auto key = workspace_id + "#" + user_id;
  std::lock_guard lock(mutex_);
  presence_[key] = PresenceInfo{.user_id = user_id, .scope = scope, .last_seen = std::chrono::steady_clock::now()};
}

auto PresenceService::GetPresence(const std::string& workspace_id) const -> std::vector<PresenceInfo> {
  std::lock_guard lock(mutex_);
  std::vector<PresenceInfo> result;
  for (const auto& [key, info] : presence_) {
    if (key.rfind(workspace_id + "#", 0) == 0) {
      result.push_back(info);
    }
  }
  return result;
}

}  // namespace cppwiki::server::service
