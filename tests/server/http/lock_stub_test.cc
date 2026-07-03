#include <spdlog/spdlog.h>

#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

#include "server/service/lock_service.h"

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto TestAcquireAndRelease() -> void {
  cppwiki::server::service::LockService locks;
  Require(locks.Acquire("doc-1", "user-a"), "first acquire should succeed");
  Require(!locks.Acquire("doc-1", "user-b"), "second owner acquire should fail");
  Require(locks.IsLocked("doc-1"), "doc should be locked");
  Require(locks.GetOwner("doc-1") == "user-a", "owner should be user-a");
  Require(!locks.Release("doc-1", "user-b"), "release by non-owner should fail");
  Require(locks.Release("doc-1", "user-a"), "release by owner should succeed");
  Require(!locks.IsLocked("doc-1"), "doc should be unlocked");
}

auto TestSameOwnerReacquire() -> void {
  cppwiki::server::service::LockService locks;
  Require(locks.Acquire("doc-1", "user-a"), "first acquire should succeed");
  Require(locks.Acquire("doc-1", "user-a"), "same owner acquire should refresh lock");
  Require(locks.Heartbeat("doc-1", "user-a"), "heartbeat by owner should succeed");
  Require(!locks.Heartbeat("doc-1", "user-b"), "heartbeat by non-owner should fail");
}

auto TestHeartbeatRequiresLock() -> void {
  cppwiki::server::service::LockService locks;
  Require(!locks.Heartbeat("doc-x", "user-a"), "heartbeat without lock should fail");
}

auto TestForceRelease() -> void {
  cppwiki::server::service::LockService locks;
  Require(locks.Acquire("doc-1", "user-a"), "acquire should succeed");
  Require(locks.ForceRelease("doc-1"), "force release should succeed");
  Require(!locks.IsLocked("doc-1"), "doc should be unlocked after force release");
  Require(!locks.ForceRelease("doc-1"), "force release of unlocked doc should fail");
}

}  // namespace

auto main() -> int {
  TestAcquireAndRelease();
  TestSameOwnerReacquire();
  TestHeartbeatRequiresLock();
  TestForceRelease();

  spdlog::info("cppwiki_server_lock_stub_tests passed");
  return EXIT_SUCCESS;
}
