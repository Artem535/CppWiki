#include "admin/admin_response_parsing.h"

#include <rfl/Rename.hpp>
#include <rfl/json.hpp>

#include <utility>

namespace cppwiki::admin {

namespace {

struct ErrorDto final {
  std::string code;
  std::string message;
};

struct ErrorEnvelopeDto final {
  rfl::Rename<"apiVersion", int> api_version{1};
  bool ok = false;
  ErrorDto error;
};

struct WorkspaceDto final {
  std::string id;
};

struct WorkspaceListResultDto final {
  std::vector<WorkspaceDto> workspaces;
};

struct WorkspaceListEnvelopeDto final {
  rfl::Rename<"apiVersion", int> api_version{1};
  bool ok = true;
  WorkspaceListResultDto result;
};

struct WorkspaceCreateResultDto final {
  WorkspaceDto workspace;
  rfl::Rename<"created", bool> created{true};
};

struct WorkspaceCreateEnvelopeDto final {
  rfl::Rename<"apiVersion", int> api_version{1};
  bool ok = true;
  WorkspaceCreateResultDto result;
};

struct AdminSyncOverviewResultDto final {
  bool available = true;
  bool enabled = false;
  rfl::Rename<"gatewayUrl", std::string> gateway_url;
  rfl::Rename<"adminUrl", std::string> admin_url;
  rfl::Rename<"databaseName", std::string> database_name;
  rfl::Rename<"statusText", std::string> status_text;
  std::vector<std::string> workspaces;
  rfl::Rename<"roleChannels", std::map<std::string, std::vector<std::string>>> role_channels;
  rfl::Rename<"groupChannels", std::map<std::string, std::vector<std::string>>> group_channels;
};

struct AdminSyncOverviewEnvelopeDto final {
  rfl::Rename<"apiVersion", int> api_version{1};
  bool ok = true;
  AdminSyncOverviewResultDto result;
};

auto TryExtractErrorMessage(const std::string& json_body) -> std::optional<std::string> {
  const auto parsed = rfl::json::read<ErrorEnvelopeDto>(json_body);
  if (!parsed) {
    return std::nullopt;
  }
  const auto& error = parsed.value().error;
  if (!error.message.empty()) {
    return error.message;
  }
  if (!error.code.empty()) {
    return error.code;
  }
  return "Request was rejected.";
}

}  // namespace

auto NormalizeBaseUrl(std::string base_url) -> std::string {
  while (!base_url.empty() && base_url.back() == '/') {
    base_url.pop_back();
  }
  return base_url;
}

auto BuildApiUrl(const std::string& base_url, const std::string& path) -> std::string {
  return NormalizeBaseUrl(base_url) + path;
}

auto BuildWorkspaceCreateBody(const std::string& workspace_id) -> std::string {
  return rfl::json::write(WorkspaceDto{.id = workspace_id});
}

auto ExtractEnvelopeError(const std::string& json_body) -> std::optional<std::string> {
  return TryExtractErrorMessage(json_body);
}

auto ParseWorkspaceIds(const std::string& json_body) -> std::vector<std::string> {
  const auto parsed = rfl::json::read<WorkspaceListEnvelopeDto>(json_body);
  if (!parsed || !parsed.value().ok) {
    return {};
  }

  std::vector<std::string> ids;
  ids.reserve(parsed.value().result.workspaces.size());
  for (const auto& workspace : parsed.value().result.workspaces) {
    ids.push_back(workspace.id);
  }
  return ids;
}

auto ParseWorkspaceCreateResult(const std::string& json_body)
    -> std::optional<WorkspaceCreateResult> {
  const auto parsed = rfl::json::read<WorkspaceCreateEnvelopeDto>(json_body);
  if (!parsed || !parsed.value().ok) {
    return std::nullopt;
  }

  return WorkspaceCreateResult{
      .id = parsed.value().result.workspace.id,
      .created = parsed.value().result.created.get(),
  };
}

auto ParseAdminSyncOverview(const std::string& json_body) -> std::optional<AdminSyncOverview> {
  const auto parsed = rfl::json::read<AdminSyncOverviewEnvelopeDto>(json_body);
  if (!parsed || !parsed.value().ok) {
    return std::nullopt;
  }

  const auto& result = parsed.value().result;
  return AdminSyncOverview{
      .available = result.available,
      .enabled = result.enabled,
      .gateway_url = result.gateway_url.get(),
      .admin_url = result.admin_url.get(),
      .database_name = result.database_name.get(),
      .status_text = result.status_text.get(),
      .workspaces = result.workspaces,
      .role_channels = result.role_channels.get(),
      .group_channels = result.group_channels.get(),
  };
}

auto IsHttpSuccessStatus(long status_code) -> bool {
  return status_code >= 200 && status_code < 300;
}

}  // namespace cppwiki::admin
