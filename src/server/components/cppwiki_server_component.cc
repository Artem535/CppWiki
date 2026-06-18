#include "server/components/cppwiki_server_component.h"

#include <memory>

#include <userver/server/handlers/auth/auth_checker_factory.hpp>
#include <userver/server/handlers/auth/handler_auth_config.hpp>

#include "server/handlers/health_handler.h"
#include "server/handlers/lock_handler.h"
#include "server/handlers/options_handler.h"
#include "server/handlers/presence_handler.h"
#include "server/handlers/protected_page_handler.h"
#include "server/middleware/auth_checker_impl.h"

namespace cppwiki::server::components {

namespace {

class AuthCheckerFactory final : public userver::server::handlers::auth::AuthCheckerFactoryBase {
 public:
  static constexpr std::string_view kAuthType = "cppwiki-auth-checker";

  explicit AuthCheckerFactory(const userver::components::ComponentContext&) {}

  [[nodiscard]] userver::server::handlers::auth::AuthCheckerBasePtr MakeAuthChecker(
      const userver::server::handlers::auth::HandlerAuthConfig&) const override {
    return std::make_shared<middleware::AuthCheckerImpl>();
  }
};

}  // namespace

auto GetStaticConfigComponentName() -> const std::string& {
  static const std::string kName("cppwiki-static-config");
  return kName;
}

auto RegisterCppWikiComponents(userver::components::ComponentList& component_list)
    -> userver::components::ComponentList& {
  component_list.Append<handlers::HealthHandler>();
  component_list.Append<handlers::OptionsHandler>();
  component_list.Append<handlers::LockHandler>();
  component_list.Append<handlers::PresenceHandler>();
  component_list.Append<handlers::ProtectedPageHandler>();

  static const auto kAuthCheckerRegistration =
      (userver::server::handlers::auth::RegisterAuthCheckerFactory<AuthCheckerFactory>(), true);
  (void)kAuthCheckerRegistration;

  return component_list;
}

}  // namespace cppwiki::server::components
