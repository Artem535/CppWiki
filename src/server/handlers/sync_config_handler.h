#ifndef CPPWIKI_SRC_SERVER_HANDLERS_SYNC_CONFIG_HANDLER_H_
#define CPPWIKI_SRC_SERVER_HANDLERS_SYNC_CONFIG_HANDLER_H_

#include <string>

#include <userver/clients/http/client.hpp>
#include <userver/server/handlers/http_handler_json_base.hpp>

#include "server/components/sync_bootstrap_component.h"
#include "server/service/sync_gateway_adapter.h"

namespace cppwiki::server::handlers {

class SyncConfigHandler final : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-sync-config";

  SyncConfigHandler(const userver::components::ComponentConfig& config,
                    const userver::components::ComponentContext& context);

  [[nodiscard]] auto HandleRequestJsonThrow(
      const userver::server::http::HttpRequest& request,
      const userver::formats::json::Value& request_body,
      userver::server::request::RequestContext& context) const
      -> userver::formats::json::Value override;

 private:
  [[nodiscard]] auto ProbeGatewayStatus() const -> std::string;

  const components::SyncBootstrapComponent& sync_config_;
  userver::clients::http::Client& http_client_;
  service::SyncGatewayAdapter sync_gateway_adapter_;
};

}  // namespace cppwiki::server::handlers

#endif  // CPPWIKI_SRC_SERVER_HANDLERS_SYNC_CONFIG_HANDLER_H_
