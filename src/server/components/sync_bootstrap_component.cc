#include "server/components/sync_bootstrap_component.h"

#include "core/constants.h"

#include <algorithm>
#include <cctype>
#include <rfl/json/read.hpp>
#include <rfl/json/write.hpp>
#include <string_view>

#include <spdlog/spdlog.h>

#include <userver/clients/http/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace cppwiki::server::components {

namespace {

constexpr std::string_view kWorkspaceChannelPrefix = "workspace:";
constexpr std::string_view kRegistryDocumentId = "system:workspaces";
constexpr std::string_view kRegistryChannel = "system:registry";
constexpr std::string_view kDefaultScopeName = "_default";

struct WorkspaceStoreDocument final {
  std::optional<rfl::Rename<"_id", std::string>> id;
  std::optional<rfl::Rename<"_rev", std::string>> rev;
  std::vector<std::string> workspaces;
  std::vector<std::string> channels;
};

auto NormalizeWorkspaceId(std::string workspace_id) -> std::string {
  workspace_id.erase(
      std::remove_if(workspace_id.begin(), workspace_id.end(),
                     [](unsigned char ch) { return std::isspace(ch) != 0; }),
      workspace_id.end());
  return workspace_id.empty() ? std::string("default") : workspace_id;
}

void AppendWorkspaceChannels(const std::map<std::string, std::vector<std::string>>& mapping,
                             std::set<std::string>& workspaces) {
  for (const auto& [_, channels] : mapping) {
    for (const auto& channel : channels) {
      if (!channel.starts_with(kWorkspaceChannelPrefix)) {
        continue;
      }
      auto workspace_id = channel.substr(kWorkspaceChannelPrefix.size());
      if (!workspace_id.empty()) {
        workspaces.insert(NormalizeWorkspaceId(std::move(workspace_id)));
      }
    }
  }
}

}  // namespace

SyncBootstrapComponent::SyncBootstrapComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      http_client_(context.FindComponent<userver::components::HttpClient>().GetHttpClient()) {
  const auto configured_enabled = config["enabled"].As<bool>(false);
  state_.gateway_url = config["gateway_url"].As<std::string>("");
  state_.database_name = config["database_name"].As<std::string>("cppwiki");
  state_.admin_url = config["admin_url"].As<std::string>("");
  state_.role_channels =
      config["role_channels"].As<std::map<std::string, std::vector<std::string>>>({});
  state_.group_channels =
      config["group_channels"].As<std::map<std::string, std::vector<std::string>>>({});

  if (!configured_enabled) {
    state_.available = true;
    state_.enabled = false;
    state_.status_text = "Sync is disabled by server";
  } else if (state_.gateway_url.empty()) {
    state_.available = true;
    state_.enabled = false;
    state_.status_text = "Sync is enabled, but gateway URL is not configured";
  } else {
    state_.available = true;
    state_.enabled = true;
    state_.status_text = "Sync bootstrap is ready";
  }

  workspace_ids_ = DeriveInitialWorkspaces(state_);
  LoadPersistedWorkspaces();
}

auto SyncBootstrapComponent::GetState() const -> const SyncBootstrapState& {
  return state_;
}

auto SyncBootstrapComponent::ListWorkspaces() const -> std::vector<std::string> {
  std::lock_guard lock(workspaces_mutex_);
  return {workspace_ids_.begin(), workspace_ids_.end()};
}

auto SyncBootstrapComponent::AddWorkspace(std::string workspace_id) -> AddWorkspaceResult {
  workspace_id = NormalizeWorkspaceId(std::move(workspace_id));
  std::lock_guard lock(workspaces_mutex_);
  const auto [it, inserted] = workspace_ids_.insert(workspace_id);
  if (!inserted) {
    return AddWorkspaceResult::kAlreadyExists;
  }
  if (!PersistWorkspacesLocked()) {
    workspace_ids_.erase(it);
    return AddWorkspaceResult::kPersistFailed;
  }

  // Materialize the workspace as a real synced root/meta document so that
  // desktop clients can prove initial-pull completion by the fact of this
  // document's presence, rather than inferring it from replicator idle
  // state or from the (potentially empty) page list.
  if (!PersistWorkspaceRootDocument(workspace_id)) {
    spdlog::error(
        "Workspace '{}' was registered but its root/meta sync document could not be created; "
        "desktop clients will not be able to prove hydration for this workspace.",
        workspace_id);
  }

  return AddWorkspaceResult::kAdded;
}

auto SyncBootstrapComponent::DeriveInitialWorkspaces(const SyncBootstrapState& state)
    -> std::set<std::string> {
  std::set<std::string> workspaces{std::string("default")};
  AppendWorkspaceChannels(state.role_channels, workspaces);
  AppendWorkspaceChannels(state.group_channels, workspaces);
  return workspaces;
}

void SyncBootstrapComponent::LoadPersistedWorkspaces() {
  const auto registry_url = RegistryDocumentUrl();
  if (registry_url.empty()) {
    return;
  }

  try {
    const auto response = http_client_.CreateRequest()
                              .get(registry_url)
                              .timeout(std::chrono::seconds(2))
                              .SetDestinationMetricName("sync-gateway-workspaces-read")
                              .perform();
    if (response->status_code() == 404) {
      return;
    }
    if (!response->IsOk()) {
      spdlog::error("Failed to load workspace registry from {}: HTTP {}", registry_url,
                    static_cast<int>(response->status_code()));
      return;
    }

    auto parsed = rfl::json::read<WorkspaceStoreDocument>(response->body());
    if (!parsed) {
      spdlog::error("Failed to parse workspace registry {}: {}", registry_url,
                    parsed.error().what());
      return;
    }

    std::lock_guard lock(workspaces_mutex_);
    for (auto workspace_id : parsed.value().workspaces) {
      workspace_ids_.insert(NormalizeWorkspaceId(std::move(workspace_id)));
    }
  } catch (const std::exception& exception) {
    spdlog::error("Failed to load workspace registry from {}: {}", registry_url,
                  exception.what());
    return;
  }
}

auto SyncBootstrapComponent::PersistWorkspacesLocked() -> bool {
  const auto registry_url = RegistryDocumentUrl();
  if (registry_url.empty()) {
    return true;
  }

  try {
    std::optional<std::string> revision;
    const auto current_response = http_client_.CreateRequest()
                                      .get(registry_url)
                                      .timeout(std::chrono::seconds(2))
                                      .SetDestinationMetricName("sync-gateway-workspaces-read")
                                      .perform();

    if (current_response->status_code() == 200) {
      auto parsed = rfl::json::read<WorkspaceStoreDocument>(current_response->body());
      if (!parsed) {
        spdlog::error("Failed to parse existing workspace registry {}: {}", registry_url,
                      parsed.error().what());
        return false;
      }

      revision = parsed.value().rev ? std::optional<std::string>{parsed.value().rev->value()}
                                    : std::nullopt;
      for (auto workspace_id : parsed.value().workspaces) {
        workspace_ids_.insert(NormalizeWorkspaceId(std::move(workspace_id)));
      }
    } else if (current_response->status_code() != 404) {
      spdlog::error("Failed to read existing workspace registry from {}: HTTP {}", registry_url,
                    static_cast<int>(current_response->status_code()));
      return false;
    }

    WorkspaceStoreDocument file{
        .id = rfl::Rename<"_id", std::string>{std::string(kRegistryDocumentId)},
        .rev = revision ? std::optional<rfl::Rename<"_rev", std::string>>{
                              rfl::Rename<"_rev", std::string>{*revision}}
                        : std::nullopt,
        .workspaces = {workspace_ids_.begin(), workspace_ids_.end()},
        .channels = {std::string(kRegistryChannel)},
    };
    const auto payload = rfl::json::write(file);
    const auto response = http_client_.CreateRequest()
                              .put(registry_url, payload)
                              .headers({{"Content-Type", "application/json"}})
                              .timeout(std::chrono::seconds(2))
                              .SetDestinationMetricName("sync-gateway-workspaces-write")
                              .perform();
    if (!response->IsOk()) {
      spdlog::error("Failed to persist workspace registry to {}: HTTP {}", registry_url,
                    static_cast<int>(response->status_code()));
      return false;
    }
    return true;
  } catch (const std::exception& exception) {
    spdlog::error("Failed to persist workspace registry to {}: {}", registry_url,
                  exception.what());
    return false;
  }
}

auto SyncBootstrapComponent::KeyspaceDocumentUrl(std::string_view document_id) const
    -> std::string {
  if (state_.admin_url.empty()) {
    return {};
  }

  auto url = state_.admin_url;
  if (url.ends_with('/')) {
    url.pop_back();
  }

  const auto slash_pos = url.find_last_of('/');
  if (slash_pos == std::string::npos || slash_pos + 1 >= url.size()) {
    return {};
  }

  auto keyspace = url.substr(slash_pos + 1);
  if (keyspace.find('.') == std::string::npos) {
    keyspace += "." + std::string(kDefaultScopeName) + "." +
                std::string(cppwiki::constants::kDocumentsCollectionName);
  }

  return url.substr(0, slash_pos + 1) + keyspace + "/" + std::string(document_id);
}

auto SyncBootstrapComponent::RegistryDocumentUrl() const -> std::string {
  return KeyspaceDocumentUrl(kRegistryDocumentId);
}

auto SyncBootstrapComponent::PersistWorkspaceRootDocument(const std::string& workspace_id)
    -> bool {
  const auto document_id =
      std::string(cppwiki::constants::kWorkspaceDocumentIdPrefix) + workspace_id;
  const auto document_url = KeyspaceDocumentUrl(document_id);
  if (document_url.empty()) {
    // No admin URL configured (e.g. local/dev without Sync Gateway admin API) -
    // nothing to materialize server-side; desktop clients fall back to their
    // own local bootstrap of the workspace root on first connect.
    return true;
  }

  try {
    std::optional<std::string> revision;
    const auto current_response = http_client_.CreateRequest()
                                       .get(document_url)
                                       .timeout(std::chrono::seconds(2))
                                       .SetDestinationMetricName("sync-gateway-workspace-root-read")
                                       .perform();
    if (current_response->status_code() == 200) {
      // Root document already exists (e.g. re-registration after restart); nothing to do.
      return true;
    }
    if (current_response->status_code() != 404) {
      spdlog::error("Failed to check workspace root document at {}: HTTP {}", document_url,
                    static_cast<int>(current_response->status_code()));
      return false;
    }

    struct WorkspaceRootDocument final {
      rfl::Rename<"_id", std::string> id;
      std::string type{"workspace"};
      rfl::Rename<"workspace_id", std::string> workspace_id;
      std::vector<std::string> channels;
    };

    const WorkspaceRootDocument payload_document{
        .id = rfl::Rename<"_id", std::string>{document_id},
        .workspace_id = rfl::Rename<"workspace_id", std::string>{workspace_id},
        .channels = {std::string(kWorkspaceChannelPrefix) + workspace_id},
    };
    const auto payload = rfl::json::write(payload_document);
    const auto response = http_client_.CreateRequest()
                               .put(document_url, payload)
                               .headers({{"Content-Type", "application/json"}})
                               .timeout(std::chrono::seconds(2))
                               .SetDestinationMetricName("sync-gateway-workspace-root-write")
                               .perform();
    if (!response->IsOk()) {
      spdlog::error("Failed to persist workspace root document to {}: HTTP {}", document_url,
                    static_cast<int>(response->status_code()));
      return false;
    }
    spdlog::info("Workspace '{}' root/meta sync document materialized at {}", workspace_id,
                 document_url);
    return true;
  } catch (const std::exception& exception) {
    spdlog::error("Failed to persist workspace root document to {}: {}", document_url,
                  exception.what());
    return false;
  }
}

auto SyncBootstrapComponent::GetStaticConfigSchema() -> userver::yaml_config::Schema {
  return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: Desktop sync bootstrap configuration exposed to authenticated clients.
additionalProperties: false
properties:
    enabled:
        type: boolean
        description: Enables sync bootstrap delivery to the desktop client.
    gateway_url:
        type: string
        description: Sync Gateway replication URL used by desktop clients.
    database_name:
        type: string
        description: Logical replicated database name exposed to the desktop client.
    admin_url:
        type: string
        description: Sync Gateway admin database URL used to persist dynamic workspace registry.
    role_channels:
        type: object
        properties: {}
        additionalProperties:
            type: array
            description: Channels granted for a single role claim.
            items:
                type: string
                description: Sync Gateway channel name.
        description: Mapping from trusted JWT role claims to Sync Gateway channels.
    group_channels:
        type: object
        properties: {}
        additionalProperties:
            type: array
            description: Channels granted for a single group claim.
            items:
                type: string
                description: Sync Gateway channel name.
        description: Mapping from trusted JWT group claims to Sync Gateway channels.
)");
}

}  // namespace cppwiki::server::components
