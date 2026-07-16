#ifndef CPPWIKI_SRC_ADMIN_ADMIN_TOKEN_STORE_H_
#define CPPWIKI_SRC_ADMIN_ADMIN_TOKEN_STORE_H_

#include <optional>
#include <string>

namespace cppwiki::admin {

// Narrow seam around a native OS secret store (libsecret on Linux, Keychain
// on macOS, Credential Manager on Windows), so AdminTokenStore's logic can be
// unit tested with a fake backend instead of touching a real secret-service
// daemon. The production implementation (SystemKeychainBackend, in
// system_keychain_backend.h/.cc) wraps the hrantzsch/keychain library.
class KeychainBackend {
 public:
  virtual ~KeychainBackend() = default;

  // Returns true and fills `out_password` if an entry was found; returns
  // false (leaving `out_password` untouched) if the entry is missing or the
  // backend reports an error.
  [[nodiscard]] virtual auto GetPassword(const std::string& package,
                                         const std::string& service,
                                         const std::string& user,
                                         std::string& out_password) -> bool = 0;

  // Returns true on success.
  [[nodiscard]] virtual auto SetPassword(const std::string& package,
                                         const std::string& service,
                                         const std::string& user,
                                         const std::string& password) -> bool = 0;

  // Returns true if the entry was deleted or did not exist; false only on a
  // genuine backend error.
  [[nodiscard]] virtual auto DeletePassword(const std::string& package,
                                            const std::string& service,
                                            const std::string& user) -> bool = 0;
};

// Stores the cppwiki-admin session token in the OS keychain via a
// KeychainBackend (hrantzsch/keychain in production), rather than a
// second, separate secret-storage mechanism — matching the
// Admin_Surface_Backlog decision to reuse a real OS secret store instead of
// a plaintext file.
class AdminTokenStore final {
 public:
  // Uses the process-wide default KeychainBackend (SystemKeychainBackend)
  // and the default account identifier (current OS user, falling back to a
  // fixed name if it cannot be determined).
  AdminTokenStore();
  explicit AdminTokenStore(KeychainBackend& backend, std::string account = DefaultAccount());

  // Returns the account identifier used to namespace the stored token
  // (defaults to the current OS user via $USER/$LOGNAME).
  [[nodiscard]] static auto DefaultAccount() -> std::string;

  // Human-readable description of where the token is stored, for status
  // messages (e.g. "the OS keychain (service: cppwiki-admin-session)").
  [[nodiscard]] static auto StorageDescription() -> std::string;

  [[nodiscard]] auto Load() const -> std::optional<std::string>;
  [[nodiscard]] auto Save(const std::string& token) const -> bool;
  [[nodiscard]] auto Clear() const -> bool;

 private:
  KeychainBackend& backend_;
  std::string account_;
};

}  // namespace cppwiki::admin

#endif  // CPPWIKI_SRC_ADMIN_ADMIN_TOKEN_STORE_H_
