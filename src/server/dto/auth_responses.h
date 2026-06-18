#ifndef CPPWIKI_SRC_SERVER_DTO_AUTH_RESPONSES_H_
#define CPPWIKI_SRC_SERVER_DTO_AUTH_RESPONSES_H_

#include <optional>
#include <string>
#include <vector>

#include <rfl.hpp>

namespace cppwiki::server::dto {

struct RefreshResponse {
  std::string access_token;
  std::string token_type = "Bearer";
  std::int64_t expires_in = 0;
};

struct UserProfileResponse {
  std::string sub;
  std::string email;
  std::string display_name;
  std::vector<std::string> roles;
};

}  // namespace cppwiki::server::dto

#endif  // CPPWIKI_SRC_SERVER_DTO_AUTH_RESPONSES_H_
