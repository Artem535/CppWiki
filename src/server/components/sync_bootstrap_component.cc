#include "server/components/sync_bootstrap_component.h"

#include <userver/components/component_config.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace cppwiki::server::components {

SyncBootstrapComponent::SyncBootstrapComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context) {
  const auto configured_enabled = config["enabled"].As<bool>(false);
  state_.gateway_url = config["gateway_url"].As<std::string>("");
  state_.database_name = config["database_name"].As<std::string>("cppwiki");
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
}

auto SyncBootstrapComponent::GetState() const -> const SyncBootstrapState& {
  return state_;
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
    role_channels:
        type: object
        additionalProperties:
            type: array
            items:
                type: string
        description: Mapping from trusted JWT role claims to Sync Gateway channels.
    group_channels:
        type: object
        additionalProperties:
            type: array
            items:
                type: string
        description: Mapping from trusted JWT group claims to Sync Gateway channels.
)");
}

}  // namespace cppwiki::server::components
