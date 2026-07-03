#include "server/handlers/sync_config_handler.h"

#include <chrono>
#include <stdexcept>
#include <utility>

#include <userver/clients/http/component.hpp>
#include <userver/components/component_context.hpp>

#include "server/dto/response_envelope.h"
#include "server/handlers/handler_utils.h"
#include "server/middleware/auth_checker_impl.h"
#include "server/middleware/logging_middleware.h"

namespace cppwiki::server::handlers {

SyncConfigHandler::SyncConfigHandler(const userver::components::ComponentConfig& config,
                                     const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      sync_config_(context.FindComponent<components::SyncBootstrapComponent>()),
      http_client_(context.FindComponent<userver::components::HttpClient>().GetHttpClient()),
      sync_gateway_adapter_(sync_config_) {}

auto SyncConfigHandler::HandleRequestJsonThrow(const userver::server::http::HttpRequest& request,
                                               const userver::formats::json::Value&,
                                               userver::server::request::RequestContext& context) const
    -> userver::formats::json::Value {
  middleware::AttachRequestTags(const_cast<userver::server::http::HttpRequest&>(request));
  const auto* principal =
      context.GetDataOptional<middleware::JwtPrincipal>(middleware::kJwtPrincipalContextKey);
  if (principal == nullptr) {
    throw std::runtime_error("JWT principal missing from sync bootstrap request context");
  }

  PrepareJsonResponse(request, "GET, OPTIONS");

  auto bootstrap = sync_gateway_adapter_.BuildBootstrap(*principal);
  if (bootstrap.enabled) {
    if (const auto gateway_status = ProbeGatewayStatus(); !gateway_status.empty()) {
      bootstrap.enabled = false;
      bootstrap.status_text = std::move(gateway_status);
    }
  }

  return dto::MakeSuccessEnvelopeJson(dto::kApiVersion, std::move(bootstrap));
}

auto SyncConfigHandler::ProbeGatewayStatus() const -> std::string {
  const auto& state = sync_config_.GetState();
  if (state.gateway_url.empty()) {
    return {};
  }

  try {
    const auto response = http_client_.CreateRequest()
                              .get(state.gateway_url)
                              .timeout(std::chrono::seconds(1))
                              .SetDestinationMetricName("sync-gateway-probe")
                              .perform();
    if (response->status_code() > 0) {
      return {};
    }
  } catch (const std::exception& exception) {
    return "Sync Gateway is unreachable at " + state.gateway_url + " (" + exception.what() + ")";
  }

  return "Sync Gateway is unreachable at " + state.gateway_url;
}

}  // namespace cppwiki::server::handlers
