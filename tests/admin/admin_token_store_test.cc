#include "admin/admin_token_store.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <map>
#include <string>
#include <string_view>
#include <tuple>

namespace {

using cppwiki::admin::AdminTokenStore;
using cppwiki::admin::KeychainBackend;

// Real keychain access (libsecret/Keychain Services/Credential Manager)
// cannot be meaningfully exercised in CI without a running secret-service
// daemon, so this fake stands in for SystemKeychainBackend to test
// AdminTokenStore's own Load/Save/Clear logic in isolation.
class FakeKeychainBackend final : public KeychainBackend {
 public:
  [[nodiscard]] auto GetPassword(const std::string& package, const std::string& service,
                                 const std::string& user, std::string& out_password)
      -> bool override {
    ++get_calls;
    const auto it = entries_.find(std::make_tuple(package, service, user));
    if (it == entries_.end()) {
      return false;
    }
    out_password = it->second;
    return true;
  }

  [[nodiscard]] auto SetPassword(const std::string& package, const std::string& service,
                                 const std::string& user, const std::string& password)
      -> bool override {
    ++set_calls;
    if (fail_set) {
      return false;
    }
    entries_[std::make_tuple(package, service, user)] = password;
    return true;
  }

  [[nodiscard]] auto DeletePassword(const std::string& package, const std::string& service,
                                    const std::string& user) -> bool override {
    ++delete_calls;
    entries_.erase(std::make_tuple(package, service, user));
    return true;
  }

  int get_calls = 0;
  int set_calls = 0;
  int delete_calls = 0;
  bool fail_set = false;

 private:
  std::map<std::tuple<std::string, std::string, std::string>, std::string> entries_;
};

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto TestLoadReturnsNulloptWhenMissing() -> void {
  FakeKeychainBackend backend;
  AdminTokenStore store(backend, "alice");
  Require(!store.Load().has_value(), "Load should return nullopt for a missing entry");
  Require(backend.get_calls == 1, "Load should call GetPassword exactly once");
}

auto TestSaveThenLoadRoundTrips() -> void {
  FakeKeychainBackend backend;
  AdminTokenStore store(backend, "alice");
  Require(store.Save("token-123"), "Save should succeed");
  const auto loaded = store.Load();
  Require(loaded.has_value() && *loaded == "token-123",
          "Load should return the token that was saved");
}

auto TestSaveFailurePropagates() -> void {
  FakeKeychainBackend backend;
  backend.fail_set = true;
  AdminTokenStore store(backend, "alice");
  Require(!store.Save("token-123"), "Save should report failure from the backend");
}

auto TestClearRemovesEntry() -> void {
  FakeKeychainBackend backend;
  AdminTokenStore store(backend, "alice");
  Require(store.Save("token-123"), "Save should succeed before clearing");
  Require(store.Clear(), "Clear should succeed");
  Require(!store.Load().has_value(), "Load should return nullopt after Clear");
}

auto TestAccountsAreIsolated() -> void {
  FakeKeychainBackend backend;
  AdminTokenStore alice_store(backend, "alice");
  AdminTokenStore bob_store(backend, "bob");
  Require(alice_store.Save("alice-token"), "Save should succeed for alice");
  Require(!bob_store.Load().has_value(),
          "A different account should not see another account's stored token");
}

auto TestDefaultAccountIsNonEmpty() -> void {
  Require(!AdminTokenStore::DefaultAccount().empty(),
          "DefaultAccount should always return a non-empty identifier");
}

}  // namespace

auto main() -> int {
  TestLoadReturnsNulloptWhenMissing();
  TestSaveThenLoadRoundTrips();
  TestSaveFailurePropagates();
  TestClearRemovesEntry();
  TestAccountsAreIsolated();
  TestDefaultAccountIsNonEmpty();

  spdlog::info("cppwiki_admin_token_store_tests passed");
  return EXIT_SUCCESS;
}
