#ifndef CPPWIKI_SRC_SERVER_DTO_SYNC_CONFIG_RESPONSE_H_
#define CPPWIKI_SRC_SERVER_DTO_SYNC_CONFIG_RESPONSE_H_

#include <rfl/Rename.hpp>

#include <string>
#include <vector>

namespace cppwiki::server::dto {

struct SyncAuthConfigDto final {
  rfl::Rename<"mode", std::string> mode;
  rfl::Rename<"tokenPassthrough", bool> token_passthrough{true};
};

struct SyncPrincipalDto final {
  std::string subject;
  rfl::Rename<"preferredUsername", std::string> preferred_username;
  std::string email;
  std::vector<std::string> roles;
  std::vector<std::string> groups;
};

struct SyncConfigResultDto final {
  bool available{true};
  bool enabled{false};
  rfl::Rename<"gatewayUrl", std::string> gateway_url;
  rfl::Rename<"databaseName", std::string> database_name;
  SyncAuthConfigDto auth;
  SyncPrincipalDto principal;
  std::vector<std::string> channels;
  rfl::Rename<"statusText", std::string> status_text;
};

}  // namespace cppwiki::server::dto

#endif  // CPPWIKI_SRC_SERVER_DTO_SYNC_CONFIG_RESPONSE_H_
