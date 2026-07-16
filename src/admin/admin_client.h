#ifndef CPPWIKI_SRC_ADMIN_ADMIN_CLIENT_H_
#define CPPWIKI_SRC_ADMIN_ADMIN_CLIENT_H_

#include <string>
#include <vector>

#include "admin/admin_http_client.h"
#include "admin/admin_response_parsing.h"

namespace cppwiki::admin {

template <typename T>
struct Outcome final {
  bool success = false;
  T value{};
  std::string error_message;
};

// High-level admin API client: combines AdminHttpClient (transport) with the
// pure parsing helpers in admin_response_parsing.h (envelope/DTO shapes) to
// expose the operations cppwiki-admin's TUI needs against
// src/server/handlers/admin_handler.cc and workspace_handler.cc.
class AdminClient final {
 public:
  AdminClient(std::string base_url, std::string access_token);

  void SetAccessToken(std::string access_token);
  void SetBaseUrl(std::string base_url);

  [[nodiscard]] auto FetchSyncOverview() const -> Outcome<AdminSyncOverview>;
  [[nodiscard]] auto ListWorkspaces() const -> Outcome<std::vector<std::string>>;
  [[nodiscard]] auto CreateWorkspace(const std::string& workspace_id) const
      -> Outcome<WorkspaceCreateResult>;

 private:
  [[nodiscard]] auto DescribeFailure(const AdminHttpClient::Response& response) const
      -> std::string;

  AdminHttpClient http_client_;
};

}  // namespace cppwiki::admin

#endif  // CPPWIKI_SRC_ADMIN_ADMIN_CLIENT_H_
