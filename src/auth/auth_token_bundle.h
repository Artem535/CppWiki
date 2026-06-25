#ifndef CPPWIKI_SRC_AUTH_AUTH_TOKEN_BUNDLE_H_
#define CPPWIKI_SRC_AUTH_AUTH_TOKEN_BUNDLE_H_

#include <string>

namespace cppwiki::auth {

struct AuthTokenBundle final {
  std::string access_token;
  std::string refresh_token;
  std::string id_token;
};

}  // namespace cppwiki::auth

#endif  // CPPWIKI_SRC_AUTH_AUTH_TOKEN_BUNDLE_H_
