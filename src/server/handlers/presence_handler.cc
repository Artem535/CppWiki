#include "server/handlers/presence_handler.h"

#include <spdlog/spdlog.h>

#include "server/dto/json_adapter.h"
#include "server/dto/presence_response.h"
#include "server/dto/response_envelope.h"
#include "server/handlers/handler_utils.h"
#include "server/middleware/auth_checker_impl.h"

namespace cppwiki::server::handlers {

PresenceHandler::PresenceHandler(const userver::components::ComponentConfig& config,
                                 const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context) {}

auto PresenceHandler::HandleRequestJsonThrow(const userver::server::http::HttpRequest& request,
                                             const userver::formats::json::Value& request_body,
                                             userver::server::request::RequestContext& context) const
    -> userver::formats::json::Value {
  PrepareJsonResponse(request, "GET, POST, OPTIONS");
  const auto workspace_id = std::string(request.GetPathArg("workspace_id"));

  if (request.GetMethod() == userver::server::http::HttpMethod::kPost) {
    const auto heartbeat_request = dto::ParseJsonBody<dto::PresenceHeartbeatRequestDto>(request_body);
    const auto user_id = ExtractUserId(heartbeat_request, context);
    const auto scope =
        heartbeat_request && heartbeat_request->scope ? *heartbeat_request->scope : "view";
    presence_service_.Heartbeat(workspace_id, user_id, scope);
    spdlog::info("Presence heartbeat workspace_id={} user_id={} scope={}", workspace_id, user_id,
                 scope);
  }

  return dto::MakeSuccessEnvelopeJson(dto::kApiVersion,
                                      dto::MakePresenceResult(workspace_id,
                                                              presence_service_.GetPresence(workspace_id)));
}

auto PresenceHandler::ExtractUserId(
    const std::optional<dto::PresenceHeartbeatRequestDto>& heartbeat_request,
    const userver::server::request::RequestContext& context) -> std::string {
  if (heartbeat_request && heartbeat_request->user_id.value()) {
    return *heartbeat_request->user_id.value();
  }
  if (const auto principal =
          context.GetDataOptional<middleware::JwtPrincipal>(middleware::kJwtPrincipalContextKey)) {
    if (!principal->preferred_username.empty()) {
      return principal->preferred_username;
    }
    if (!principal->subject.empty()) {
      return principal->subject;
    }
  }
  return "anonymous";
}

}  // namespace cppwiki::server::handlers
