#include "server/components/cppwiki_server_component.h"

#include <memory>
#include <userver/clients/http/component.hpp>
#include <userver/server/handlers/auth/auth_checker_factory.hpp>
#include <userver/server/handlers/auth/handler_auth_config.hpp>

#include "server/handlers/health_handler.h"
#include "server/handlers/lock_handler.h"
#include "server/handlers/openapi_handler.h"
#include "server/handlers/options_handler.h"
#include "server/handlers/presence_handler.h"
#include "server/handlers/protected_page_handler.h"
#include "server/handlers/swagger_ui_handler.h"
#include "server/handlers/sync_config_handler.h"
#include "server/handlers/workspace_handler.h"
#include "server/components/sync_bootstrap_component.h"
#include "server/middleware/auth_checker_impl.h"

namespace cppwiki::server::components {

namespace {

class AuthCheckerFactory final : public userver::server::handlers::auth::AuthCheckerFactoryBase {
 public:
  static constexpr std::string_view kAuthType = "cppwiki-auth-checker";

  explicit AuthCheckerFactory(const userver::components::ComponentContext& context)
      : http_client_(context.FindComponent<userver::components::HttpClient>().GetHttpClient()) {}

  [[nodiscard]] auto MakeAuthChecker(
      const userver::server::handlers::auth::HandlerAuthConfig& config)
      const -> userver::server::handlers::auth::AuthCheckerBasePtr override {
    return std::make_shared<middleware::AuthCheckerImpl>(
        http_client_, middleware::JwtAuthConfig::FromHandlerAuthConfig(config));
  }

 private:
  userver::clients::http::Client& http_client_;
};

}  // namespace

auto RegisterCppWikiComponents(userver::components::ComponentList& component_list,
                               bool swagger_enabled)
    -> userver::components::ComponentList& {
  component_list.Append<handlers::HealthHandler>();
  component_list.Append<handlers::OptionsHandler>();
  if (swagger_enabled) {
    component_list.Append<handlers::OpenApiHandler>();
    component_list.Append<handlers::SwaggerUiHandler>();
  }
  component_list.Append<handlers::LockHandler>();
  component_list.Append<handlers::PresenceHandler>();
  component_list.Append<SyncBootstrapComponent>();
  component_list.Append<handlers::SyncConfigHandler>();
  component_list.Append<handlers::WorkspaceHandler>();
  component_list.Append<handlers::ProtectedPageHandler>();

  static const auto kAuthCheckerRegistration =
      (userver::server::handlers::auth::RegisterAuthCheckerFactory<AuthCheckerFactory>(), true);
  (void)kAuthCheckerRegistration;

  return component_list;
}

}  // namespace cppwiki::server::components
