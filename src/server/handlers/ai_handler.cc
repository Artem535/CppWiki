#include "server/handlers/ai_handler.h"

#include <spdlog/spdlog.h>

#include <userver/components/component_context.hpp>

#include "server/components/ai_config_component.h"
#include "server/dto/ai_response.h"
#include "server/dto/json_adapter.h"
#include "server/dto/response_envelope.h"
#include "server/handlers/handler_utils.h"

namespace cppwiki::server::handlers {

AiHandler::AiHandler(const userver::components::ComponentConfig& config,
                     const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      ai_chat_service_(
          context.FindComponent<components::AiConfigComponent>().MakeService()) {}

auto AiHandler::HandleRequestJsonThrow(const userver::server::http::HttpRequest& request,
                                      const userver::formats::json::Value& request_body,
                                      userver::server::request::RequestContext& /*context*/) const
    -> userver::formats::json::Value {
  auto& response = request.GetHttpResponse();
  PrepareJsonResponse(request, "POST, OPTIONS");

  if (!ai_chat_service_.IsConfigured()) {
    response.SetStatus(userver::server::http::HttpStatus::kServiceUnavailable);
    return dto::MakeErrorEnvelopeJson(
        dto::kApiVersion,
        dto::ErrorDto{"ai_backend_not_configured",
                      "The server-mediated AI backend is not configured."});
  }

  const auto parsed = dto::ParseJsonBody<dto::AiChatRequestDto>(request_body);
  if (!parsed || parsed->prompt.empty()) {
    response.SetStatus(userver::server::http::HttpStatus::kBadRequest);
    return dto::MakeErrorEnvelopeJson(dto::kApiVersion,
                                      dto::ErrorDto{"invalid_request", "prompt is required"});
  }

  spdlog::info("AI chat request received: mode={}", parsed->mode.value_or("rewrite"));

  try {
    const auto result = ai_chat_service_.Complete(*parsed);
    return dto::MakeSuccessEnvelopeJson(dto::kApiVersion, result);
  } catch (const std::exception& exception) {
    spdlog::error("AI chat request failed: {}", exception.what());
    response.SetStatus(userver::server::http::HttpStatus::kBadGateway);
    return dto::MakeErrorEnvelopeJson(dto::kApiVersion,
                                      dto::ErrorDto{"ai_provider_error", exception.what()});
  }
}

}  // namespace cppwiki::server::handlers
