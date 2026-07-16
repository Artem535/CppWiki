#ifndef CPPWIKI_SRC_SERVER_COMPONENTS_AI_CONFIG_COMPONENT_H_
#define CPPWIKI_SRC_SERVER_COMPONENTS_AI_CONFIG_COMPONENT_H_

#include <userver/clients/http/client.hpp>
#include <userver/components/component_base.hpp>
#include <userver/yaml_config/schema.hpp>

#include "server/service/ai_chat_service.h"

namespace cppwiki::server::components {

// Holds the AI provider configuration (base URL, API key, model) parsed from
// config/server.yaml's `ai:` section, and the HTTP client used to reach it.
// Kept separate from AiHandler so the provider secret lives in one place,
// following the same shape as SyncBootstrapComponent for sync config.
class AiConfigComponent final : public userver::components::ComponentBase {
 public:
  static constexpr std::string_view kName = "ai-config";

  AiConfigComponent(const userver::components::ComponentConfig& config,
                    const userver::components::ComponentContext& context);

  [[nodiscard]] static auto GetStaticConfigSchema() -> userver::yaml_config::Schema;
  [[nodiscard]] auto MakeService() const -> service::AiChatService;

 private:
  userver::clients::http::Client& http_client_;
  service::AiProviderConfig config_;
};

}  // namespace cppwiki::server::components

#endif  // CPPWIKI_SRC_SERVER_COMPONENTS_AI_CONFIG_COMPONENT_H_
