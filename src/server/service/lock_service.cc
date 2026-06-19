#include "server/service/lock_service.h"

#include <chrono>
#include <optional>
#include <string>

namespace cppwiki::server::service {

auto LockService::Acquire(const std::string& document_id, const std::string& owner) -> bool {
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard lock(mutex_);

  if (auto it = locks_.find(document_id); it != locks_.end()) {
    if (it->second.owner != owner) {
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

  if (auto it = locks_.find(document_id); it != locks_.end()) {
    if (it->second.owner != owner) {
      return false;
    }
    it->second.heartbeat_at = now;
    return true;
  }

  return false;
}

auto LockService::Release(const std::string& document_id, const std::string& owner) -> bool {
  std::lock_guard lock(mutex_);

  if (auto it = locks_.find(document_id); it != locks_.end()) {
    if (it->second.owner != owner) {
      return false;
    }
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
  return locks_.find(document_id) != locks_.end();
}

auto LockService::GetOwner(const std::string& document_id) const -> std::optional<std::string> {
  std::lock_guard lock(mutex_);
  if (auto it = locks_.find(document_id); it != locks_.end()) {
    return it->second.owner;
  }
  return std::nullopt;
}

}  // namespace cppwiki::server::service
