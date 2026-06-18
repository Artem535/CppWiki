#ifndef CPPWIKI_SRC_SERVER_DTO_PRESENCE_RESPONSES_H_
#define CPPWIKI_SRC_SERVER_DTO_PRESENCE_RESPONSES_H_

#include <string>
#include <vector>

#include <rfl.hpp>

namespace cppwiki::server::dto {

struct PresentUser {
  std::string user_id;
  std::string display_name;
  std::string last_seen_at;
};

struct PresenceHeartbeatResponse {
  std::string page_id;
  std::string user_id;
  std::int64_t heartbeat_interval_seconds = 10;
};

struct PresenceListResponse {
  std::string page_id;
  std::vector<PresentUser> users;
};

}  // namespace cppwiki::server::dto

#endif  // CPPWIKI_SRC_SERVER_DTO_PRESENCE_RESPONSES_H_
