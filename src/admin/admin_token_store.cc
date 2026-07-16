#include "admin/admin_token_store.h"

#include <cstdlib>
#include <utility>

#include "admin/system_keychain_backend.h"

namespace cppwiki::admin {

namespace {

constexpr const char* kKeychainPackage = "com.cppwiki.admin";
constexpr const char* kKeychainService = "cppwiki-admin-session";
constexpr const char* kFallbackAccount = "cppwiki-admin";

auto DefaultKeychainBackend() -> KeychainBackend& {
  static SystemKeychainBackend backend;
  return backend;
}

}  // namespace

AdminTokenStore::AdminTokenStore()
    : AdminTokenStore(DefaultKeychainBackend(), DefaultAccount()) {}

AdminTokenStore::AdminTokenStore(KeychainBackend& backend, std::string account)
    : backend_(backend), account_(std::move(account)) {}

auto AdminTokenStore::DefaultAccount() -> std::string {
  if (const char* user = std::getenv("USER"); user != nullptr && *user != '\0') {
    return user;
  }
  if (const char* login_name = std::getenv("LOGNAME");
      login_name != nullptr && *login_name != '\0') {
    return login_name;
  }
  return kFallbackAccount;
}

auto AdminTokenStore::StorageDescription() -> std::string {
  return std::string("the OS keychain (service: ") + kKeychainService + ")";
}

auto AdminTokenStore::Load() const -> std::optional<std::string> {
  std::string password;
  if (!backend_.GetPassword(kKeychainPackage, kKeychainService, account_, password)) {
    return std::nullopt;
  }
  if (password.empty()) {
    return std::nullopt;
  }
  return password;
}

auto AdminTokenStore::Save(const std::string& token) const -> bool {
  return backend_.SetPassword(kKeychainPackage, kKeychainService, account_, token);
}

auto AdminTokenStore::Clear() const -> bool {
  return backend_.DeletePassword(kKeychainPackage, kKeychainService, account_);
}

}  // namespace cppwiki::admin
