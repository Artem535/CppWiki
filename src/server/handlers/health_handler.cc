#include "server/handlers/health_handler.h"

#include <spdlog/spdlog.h>

#include <userver/server/http/http_response.hpp>

#include "core/constants.h"
#include "server/dto/health_response.h"
#include "server/dto/response_envelope.h"
#include "server/middleware/logging_middleware.h"

namespace cppwiki::server::handlers {

HealthHandler::HealthHandler(const userver::components::ComponentConfig& config,
                             const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context) {}

auto HealthHandler::HandleRequestJsonThrow(const userver::server::http::HttpRequest& request,
                                           const userver::formats::json::Value&,
                                           userver::server::request::RequestContext&) const
    -> userver::formats::json::Value {
  auto& response = request.GetHttpResponse();
  ApplyCorsHeaders(response);
  middleware::AttachRequestTags(const_cast<userver::server::http::HttpRequest&>(request));

  const auto result = dto::MakeHealthResult();
  spdlog::info("Health check ok");
  return dto::MakeSuccessEnvelopeJson(dto::kApiVersion, result);
}

void HealthHandler::ApplyCorsHeaders(userver::server::http::HttpResponse& response) {
  response.SetHeader(std::string_view{"Access-Control-Allow-Origin"}, "*");
  response.SetHeader(std::string_view{"Access-Control-Allow-Methods"}, "GET, OPTIONS");
  response.SetHeader(std::string_view{"Access-Control-Allow-Headers"},
                     "Content-Type, Authorization");
}

}  // namespace cppwiki::server::handlers
