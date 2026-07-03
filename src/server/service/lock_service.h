#ifndef CPPWIKI_SRC_SERVER_SERVICE_LOCK_SERVICE_H_
#define CPPWIKI_SRC_SERVER_SERVICE_LOCK_SERVICE_H_

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace cppwiki::server::service {

struct LockInfo final {
  std::string owner;
  std::chrono::steady_clock::time_point acquired_at;
  std::chrono::steady_clock::time_point heartbeat_at;
};

class LockService final {
 public:
  static constexpr auto kDefaultLockLease = std::chrono::seconds{30};

  explicit LockService(std::chrono::steady_clock::duration lock_lease = kDefaultLockLease)
      : lock_lease_(lock_lease) {}

  [[nodiscard]] auto Acquire(const std::string& document_id, const std::string& owner) -> bool;
  [[nodiscard]] auto Heartbeat(const std::string& document_id, const std::string& owner) -> bool;
  [[nodiscard]] auto Release(const std::string& document_id, const std::string& owner) -> bool;
  [[nodiscard]] auto ForceRelease(const std::string& document_id) -> bool;
  [[nodiscard]] auto IsLocked(const std::string& document_id) const -> bool;
  [[nodiscard]] auto GetOwner(const std::string& document_id) const -> std::optional<std::string>;

 private:
  [[nodiscard]] auto FindLock(const std::string& document_id)
      -> std::unordered_map<std::string, LockInfo>::iterator;
  [[nodiscard]] auto FindLock(const std::string& document_id) const
      -> std::unordered_map<std::string, LockInfo>::const_iterator;
  [[nodiscard]] auto IsOwnedBy(
      const std::unordered_map<std::string, LockInfo>::const_iterator& it,
      const std::string& owner) const -> bool;
  [[nodiscard]] auto IsExpired(const LockInfo& lock,
                               std::chrono::steady_clock::time_point now) const -> bool;
  auto PruneExpiredLocked(std::chrono::steady_clock::time_point now) const -> void;

  mutable std::mutex mutex_;
  mutable std::unordered_map<std::string, LockInfo> locks_;
  std::chrono::steady_clock::duration lock_lease_;
};

}  // namespace cppwiki::server::service

#endif  // CPPWIKI_SRC_SERVER_SERVICE_LOCK_SERVICE_H_
