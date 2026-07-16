#ifndef CPPWIKI_SRC_ADMIN_SYSTEM_KEYCHAIN_BACKEND_H_
#define CPPWIKI_SRC_ADMIN_SYSTEM_KEYCHAIN_BACKEND_H_

#include <string>

#include "admin/admin_token_store.h"

namespace cppwiki::admin {

// KeychainBackend implementation backed by the hrantzsch/keychain library,
// which wraps libsecret (Linux), Keychain Services (macOS) and Credential
// Manager (Windows) without requiring any Qt linkage — a better fit for the
// admin CLI's goal of staying Qt-GUI-free than reusing qt-keychain (which is
// only wired up behind the full Qt6 desktop find_package call in this repo).
class SystemKeychainBackend final : public KeychainBackend {
 public:
  [[nodiscard]] auto GetPassword(const std::string& package, const std::string& service,
                                 const std::string& user, std::string& out_password)
      -> bool override;
  [[nodiscard]] auto SetPassword(const std::string& package, const std::string& service,
                                 const std::string& user, const std::string& password)
      -> bool override;
  [[nodiscard]] auto DeletePassword(const std::string& package, const std::string& service,
                                    const std::string& user) -> bool override;
};

}  // namespace cppwiki::admin

#endif  // CPPWIKI_SRC_ADMIN_SYSTEM_KEYCHAIN_BACKEND_H_
