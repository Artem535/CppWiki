#ifndef CPPWIKI_SRC_SERVER_DTO_LOCK_RESPONSES_H_
#define CPPWIKI_SRC_SERVER_DTO_LOCK_RESPONSES_H_

#include <string>

#include <rfl.hpp>

namespace cppwiki::server::dto {

struct LockStateResponse {
  std::string page_id;
  bool locked = false;
  std::string owner_user_id;
  std::string acquired_at;
  std::int64_t expires_in_seconds = 0;
};

struct LockActionResponse {
  std::string page_id;
  bool acquired = false;
  bool released = false;
  std::string owner_user_id;
  std::int64_t expires_in_seconds = 0;
};

}  // namespace cppwiki::server::dto

#endif  // CPPWIKI_SRC_SERVER_DTO_LOCK_RESPONSES_H_
