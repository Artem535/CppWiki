#include "server/handlers/sync_config_handler.h"

#include <stdexcept>

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
      sync_gateway_adapter_(sync_config_.GetState()) {}

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

  return dto::MakeSuccessEnvelopeJson(dto::kApiVersion,
                                      sync_gateway_adapter_.BuildBootstrap(*principal));
}

}  // namespace cppwiki::server::handlers
