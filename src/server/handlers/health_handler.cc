#include "server/handlers/health_handler.h"

#include <spdlog/spdlog.h>

#include "core/constants.h"
#include "server/dto/health_response.h"
#include "server/dto/response_envelope.h"
#include "server/handlers/handler_utils.h"

namespace cppwiki::server::handlers {

HealthHandler::HealthHandler(const userver::components::ComponentConfig& config,
                             const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context) {}

auto HealthHandler::HandleRequestJsonThrow(const userver::server::http::HttpRequest& request,
                                           const userver::formats::json::Value&,
                                           userver::server::request::RequestContext&) const
    -> userver::formats::json::Value {
  PrepareJsonResponse(request, "GET, OPTIONS");
  const auto result = dto::MakeHealthResult();
  spdlog::info("Health check ok");
  return dto::MakeSuccessEnvelopeJson(dto::kApiVersion, result);
}

}  // namespace cppwiki::server::handlers
