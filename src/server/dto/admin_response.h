#ifndef CPPWIKI_SRC_SERVER_DTO_ADMIN_RESPONSE_H_
#define CPPWIKI_SRC_SERVER_DTO_ADMIN_RESPONSE_H_

#include <rfl/Rename.hpp>

#include <map>
#include <string>
#include <vector>

namespace cppwiki::server::dto {

struct AdminSyncOverviewResultDto final {
  bool available{true};
  bool enabled{false};
  rfl::Rename<"gatewayUrl", std::string> gateway_url;
  rfl::Rename<"adminUrl", std::string> admin_url;
  rfl::Rename<"databaseName", std::string> database_name;
  rfl::Rename<"statusText", std::string> status_text;
  std::vector<std::string> workspaces;
  rfl::Rename<"roleChannels", std::map<std::string, std::vector<std::string>>> role_channels;
  rfl::Rename<"groupChannels", std::map<std::string, std::vector<std::string>>> group_channels;
};

}  // namespace cppwiki::server::dto

#endif  // CPPWIKI_SRC_SERVER_DTO_ADMIN_RESPONSE_H_
