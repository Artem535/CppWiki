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

auto LockService::Acquire(const std::string& document_id, const std::string& owner) -> bool {
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard lock(mutex_);

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

  if (auto it = FindLock(document_id); IsOwnedBy(it, owner)) {
    it->second.heartbeat_at = now;
    return true;
  }

  return false;
}

auto LockService::Release(const std::string& document_id, const std::string& owner) -> bool {
  std::lock_guard lock(mutex_);

  if (auto it = FindLock(document_id); IsOwnedBy(it, owner)) {
    locks_.erase(it);
    return true;
  }

  return false;
}

auto LockService::ForceRelease(const std::string& document_id) -> bool {
  std::lock_guard lock(mutex_);
  return locks_.erase(document_id) > 0;
}

auto LockService::IsLocked(const std::string& document_id) const -> bool {
  std::lock_guard lock(mutex_);
  return FindLock(document_id) != locks_.end();
}

auto LockService::GetOwner(const std::string& document_id) const -> std::optional<std::string> {
  std::lock_guard lock(mutex_);
  if (auto it = FindLock(document_id); it != locks_.end()) {
    return it->second.owner;
  }
  return std::nullopt;
}

}  // namespace cppwiki::server::service
