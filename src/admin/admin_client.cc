#include "admin/admin_client.h"

#include <utility>

namespace cppwiki::admin {

AdminClient::AdminClient(std::string base_url, std::string access_token)
    : http_client_(std::move(base_url), std::move(access_token)) {}

void AdminClient::SetAccessToken(std::string access_token) {
  http_client_.SetAccessToken(std::move(access_token));
}

void AdminClient::SetBaseUrl(std::string base_url) {
  http_client_.SetBaseUrl(std::move(base_url));
}

auto AdminClient::DescribeFailure(const AdminHttpClient::Response& response) const -> std::string {
  if (!response.network_ok) {
    return "Network error: " + response.network_error;
  }

  if (response.status_code == 401 || response.status_code == 403) {
    if (const auto message = ExtractEnvelopeError(response.body); message) {
      return *message;
    }
    return "Authentication failed. Sign in as a workspace admin first.";
  }

  if (const auto message = ExtractEnvelopeError(response.body); message) {
    return *message;
  }

  return "Request failed with HTTP status " + std::to_string(response.status_code) + ".";
}

auto AdminClient::FetchSyncOverview() const -> Outcome<AdminSyncOverview> {
  const auto response = http_client_.Get("/api/v1/admin/sync");
  if (!response.network_ok || !IsHttpSuccessStatus(response.status_code)) {
    return {.success = false, .value = {}, .error_message = DescribeFailure(response)};
  }

  auto overview = ParseAdminSyncOverview(response.body);
  if (!overview) {
    return {.success = false, .value = {}, .error_message = "Could not parse server response."};
  }

  return {.success = true, .value = std::move(*overview), .error_message = {}};
}

auto AdminClient::ListWorkspaces() const -> Outcome<std::vector<std::string>> {
  const auto response = http_client_.Get("/api/v1/workspaces");
  if (!response.network_ok || !IsHttpSuccessStatus(response.status_code)) {
    return {.success = false, .value = {}, .error_message = DescribeFailure(response)};
  }

  return {.success = true, .value = ParseWorkspaceIds(response.body), .error_message = {}};
}

auto AdminClient::CreateWorkspace(const std::string& workspace_id) const
    -> Outcome<WorkspaceCreateResult> {
  const auto body = BuildWorkspaceCreateBody(workspace_id);
  const auto response = http_client_.Post("/api/v1/workspaces", body);
  if (!response.network_ok || !IsHttpSuccessStatus(response.status_code)) {
    return {.success = false, .value = {}, .error_message = DescribeFailure(response)};
  }

  auto result = ParseWorkspaceCreateResult(response.body);
  if (!result) {
    return {.success = false, .value = {}, .error_message = "Could not parse server response."};
  }

  return {.success = true, .value = std::move(*result), .error_message = {}};
}

}  // namespace cppwiki::admin
