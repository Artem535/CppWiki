#ifndef CPPWIKI_SRC_SERVER_SERVICE_PRESENCE_SERVICE_H_
#define CPPWIKI_SRC_SERVER_SERVICE_PRESENCE_SERVICE_H_

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace cppwiki::server::service {

struct PresenceInfo final {
  std::string user_id;
  std::string scope;
  std::chrono::steady_clock::time_point last_seen;
};

class PresenceService final {
 public:
  PresenceService() = default;

  auto Heartbeat(const std::string& workspace_id, const std::string& user_id,
                 const std::string& scope) -> void;

  [[nodiscard]] auto GetPresence(const std::string& workspace_id) const -> std::vector<PresenceInfo>;

 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, PresenceInfo> presence_;
};

}  // namespace cppwiki::server::service

#endif  // CPPWIKI_SRC_SERVER_SERVICE_PRESENCE_SERVICE_H_
