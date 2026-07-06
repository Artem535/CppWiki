#ifndef CPPWIKI_SRC_SERVER_HANDLERS_ADMIN_HANDLER_H_
#define CPPWIKI_SRC_SERVER_HANDLERS_ADMIN_HANDLER_H_

#include <userver/server/handlers/http_handler_json_base.hpp>

#include "server/components/sync_bootstrap_component.h"
#include "server/middleware/auth_checker_impl.h"

namespace cppwiki::server::handlers {

class AdminHandler final : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-admin-sync";

  AdminHandler(const userver::components::ComponentConfig& config,
               const userver::components::ComponentContext& context);

  [[nodiscard]] auto HandleRequestJsonThrow(
      const userver::server::http::HttpRequest& request,
      const userver::formats::json::Value& request_body,
      userver::server::request::RequestContext& context) const
      -> userver::formats::json::Value override;

 private:
  [[nodiscard]] static auto IsAdmin(const middleware::JwtPrincipal& principal) -> bool;

  const components::SyncBootstrapComponent& sync_config_;
};

}  // namespace cppwiki::server::handlers

#endif  // CPPWIKI_SRC_SERVER_HANDLERS_ADMIN_HANDLER_H_
