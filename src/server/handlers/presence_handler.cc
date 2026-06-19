#include "server/handlers/presence_handler.h"

#include <rfl/json/read.hpp>

#include <spdlog/spdlog.h>
#include <userver/formats/json.hpp>
#include <userver/server/http/http_response.hpp>

#include "server/dto/presence_response.h"
#include "server/dto/response_envelope.h"
#include "server/middleware/logging_middleware.h"

namespace cppwiki::server::handlers {

namespace {

auto ParsePresenceHeartbeatRequest(const userver::formats::json::Value& request_body)
    -> std::optional<dto::PresenceHeartbeatRequestDto> {
  auto parsed =
      rfl::json::read<dto::PresenceHeartbeatRequestDto>(userver::formats::json::ToString(request_body));
  if (!parsed) {
    return std::nullopt;
  }
  return parsed.value();
}

}  // namespace

PresenceHandler::PresenceHandler(const userver::components::ComponentConfig& config,
                                 const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context) {}

auto PresenceHandler::HandleRequestJsonThrow(const userver::server::http::HttpRequest& request,
                                             const userver::formats::json::Value& request_body,
                                             userver::server::request::RequestContext&) const
    -> userver::formats::json::Value {
  auto& response = request.GetHttpResponse();
  ApplyCorsHeaders(response);
  middleware::AttachRequestTags(const_cast<userver::server::http::HttpRequest&>(request));

  const auto workspace_id = std::string(request.GetPathArg("workspace_id"));

  if (request.GetMethod() == userver::server::http::HttpMethod::kPost) {
    const auto heartbeat_request = ParsePresenceHeartbeatRequest(request_body);
    const auto user_id =
        heartbeat_request && heartbeat_request->user_id.value() ? *heartbeat_request->user_id.value()
                                                                : "anonymous";
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

void PresenceHandler::ApplyCorsHeaders(userver::server::http::HttpResponse& response) {
  response.SetHeader(std::string_view{"Access-Control-Allow-Origin"}, "*");
  response.SetHeader(std::string_view{"Access-Control-Allow-Methods"}, "GET, POST, OPTIONS");
  response.SetHeader(std::string_view{"Access-Control-Allow-Headers"},
                     "Content-Type, Authorization");
}

}  // namespace cppwiki::server::handlers
