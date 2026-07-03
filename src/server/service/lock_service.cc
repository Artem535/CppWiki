#include "server/service/lock_service.h"

#include <chrono>
#include <optional>
#include <string>

namespace cppwiki::server::service {

auto LockService::FindLock(const std::string& document_id)
    -> std::unordered_map<std::string, LockInfo>::iterator {
  return locks_.find(document_id);
}

auto LockService::FindLock(const std::string& document_id) const
    -> std::unordered_map<std::string, LockInfo>::const_iterator {
  return locks_.find(document_id);
}

auto LockService::IsOwnedBy(const std::unordered_map<std::string, LockInfo>::const_iterator& it,
                            const std::string& owner) const -> bool {
  return it != locks_.end() && it->second.owner == owner;
}

auto LockService::IsExpired(const LockInfo& lock, std::chrono::steady_clock::time_point now) const
    -> bool {
  return lock_lease_.count() > 0 && now - lock.heartbeat_at >= lock_lease_;
}

void LockService::PruneExpiredLocked(std::chrono::steady_clock::time_point now) const {
  std::erase_if(locks_, [this, now](const auto& item) { return IsExpired(item.second, now); });
}

auto LockService::Acquire(const std::string& document_id, const std::string& owner) -> bool {
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard lock(mutex_);
  PruneExpiredLocked(now);

  if (auto it = FindLock(document_id); it != locks_.end()) {
    if (!IsOwnedBy(it, owner)) {
      return false;
    }
    it->second.heartbeat_at = now;
    return true;
  }

  locks_.emplace(document_id,
                 LockInfo{.owner = owner, .acquired_at = now, .heartbeat_at = now});
  return true;
}

auto LockService::Heartbeat(const std::string& document_id, const std::string& owner) -> bool {
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard lock(mutex_);
  PruneExpiredLocked(now);

  if (auto it = FindLock(document_id); IsOwnedBy(it, owner)) {
    it->second.heartbeat_at = now;
    return true;
  }

  return false;
}

auto LockService::Release(const std::string& document_id, const std::string& owner) -> bool {
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard lock(mutex_);
  PruneExpiredLocked(now);

  if (auto it = FindLock(document_id); IsOwnedBy(it, owner)) {
    locks_.erase(it);
    return true;
  }

  return false;
}

auto LockService::ForceRelease(const std::string& document_id) -> bool {
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard lock(mutex_);
  PruneExpiredLocked(now);
  return locks_.erase(document_id) > 0;
}

auto LockService::IsLocked(const std::string& document_id) const -> bool {
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard lock(mutex_);
  PruneExpiredLocked(now);
  return FindLock(document_id) != locks_.end();
}

auto LockService::GetOwner(const std::string& document_id) const -> std::optional<std::string> {
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard lock(mutex_);
  PruneExpiredLocked(now);
  if (auto it = FindLock(document_id); it != locks_.end()) {
    return it->second.owner;
  }
  return std::nullopt;
}

}  // namespace cppwiki::server::service
