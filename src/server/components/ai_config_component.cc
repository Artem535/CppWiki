#include "server/components/ai_config_component.h"

#include <string>

#include <userver/clients/http/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace cppwiki::server::components {

AiConfigComponent::AiConfigComponent(const userver::components::ComponentConfig& config,
                                     const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      http_client_(context.FindComponent<userver::components::HttpClient>().GetHttpClient()),
      config_(service::AiProviderConfig{
          .enabled = config["enabled"].As<bool>(false),
          .base_url = config["base_url"].As<std::string>(
              "https://api.openai.com/v1/chat/completions"),
          .api_key = config["api_key"].As<std::string>(""),
          .model = config["model"].As<std::string>(""),
      }) {}

auto AiConfigComponent::MakeService() const -> service::AiChatService {
  return service::AiChatService(http_client_, config_);
}

auto AiConfigComponent::GetStaticConfigSchema() -> userver::yaml_config::Schema {
  return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: AI provider configuration for the server-mediated BlockNote AI backend (ADR-012).
additionalProperties: false
properties:
    enabled:
        type: boolean
        description: Enables the server-mediated AI backend.
    base_url:
        type: string
        description: OpenAI-compatible chat completions endpoint URL.
    api_key:
        type: string
        description: AI provider API key, held server-side only.
    model:
        type: string
        description: AI provider model identifier.
)");
}

}  // namespace cppwiki::server::components
