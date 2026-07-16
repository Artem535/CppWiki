#include "admin/system_keychain_backend.h"

#include <keychain/keychain.h>

namespace cppwiki::admin {

auto SystemKeychainBackend::GetPassword(const std::string& package, const std::string& service,
                                        const std::string& user, std::string& out_password)
    -> bool {
  keychain::Error error;
  auto password = keychain::getPassword(package, service, user, error);
  if (error) {
    return false;
  }
  out_password = std::move(password);
  return true;
}

auto SystemKeychainBackend::SetPassword(const std::string& package, const std::string& service,
                                        const std::string& user, const std::string& password)
    -> bool {
  keychain::Error error;
  keychain::setPassword(package, service, user, password, error);
  return !error;
}

auto SystemKeychainBackend::DeletePassword(const std::string& package, const std::string& service,
                                           const std::string& user) -> bool {
  keychain::Error error;
  keychain::deletePassword(package, service, user, error);
  if (error && error.type != keychain::ErrorType::NotFound) {
    return false;
  }
  return true;
}

}  // namespace cppwiki::admin
