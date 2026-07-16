#ifndef CPPWIKI_SRC_ADMIN_ADMIN_RESPONSE_PARSING_H_
#define CPPWIKI_SRC_ADMIN_ADMIN_RESPONSE_PARSING_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cppwiki::admin {

// Pure, testable parsing/formatting logic for the cppwiki-admin CLI.
// Kept free of libcurl/ftxui so it can be unit tested without a network stack
// or terminal, mirroring the request/response shapes served by
// src/server/handlers/admin_handler.cc and src/server/handlers/workspace_handler.cc.

struct AdminSyncOverview final {
  bool available = false;
  bool enabled = false;
  std::string gateway_url;
  std::string admin_url;
  std::string database_name;
  std::string status_text;
  std::vector<std::string> workspaces;
  std::map<std::string, std::vector<std::string>> role_channels;
  std::map<std::string, std::vector<std::string>> group_channels;
};

struct WorkspaceCreateResult final {
  std::string id;
  bool created = false;
};

// Result of parsing a "{apiVersion, ok, result|error}" envelope body.
// On failure, `error_message` carries the best available diagnostic text.
template <typename T>
struct ParsedEnvelope final {
  bool ok = false;
  T result{};
  std::string error_message;
};

[[nodiscard]] auto NormalizeBaseUrl(std::string base_url) -> std::string;
[[nodiscard]] auto BuildApiUrl(const std::string& base_url, const std::string& path) -> std::string;

[[nodiscard]] auto BuildWorkspaceCreateBody(const std::string& workspace_id) -> std::string;

// Extracts the top-level "ok" flag and, on failure, the error/code/message text
// from a raw JSON response body. Returns an empty optional if the body cannot
// be parsed as JSON at all.
[[nodiscard]] auto ExtractEnvelopeError(const std::string& json_body)
    -> std::optional<std::string>;

[[nodiscard]] auto ParseWorkspaceIds(const std::string& json_body) -> std::vector<std::string>;

[[nodiscard]] auto ParseWorkspaceCreateResult(const std::string& json_body)
    -> std::optional<WorkspaceCreateResult>;

[[nodiscard]] auto ParseAdminSyncOverview(const std::string& json_body)
    -> std::optional<AdminSyncOverview>;

[[nodiscard]] auto IsHttpSuccessStatus(long status_code) -> bool;

}  // namespace cppwiki::admin

#endif  // CPPWIKI_SRC_ADMIN_ADMIN_RESPONSE_PARSING_H_
