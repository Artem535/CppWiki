#include "server/handlers/protected_page_handler.h"

#include <spdlog/spdlog.h>
#include <userver/formats/json/value_builder.hpp>

#include "server/dto/response_envelope.h"
#include "server/middleware/logging_middleware.h"

namespace cppwiki::server::handlers {

ProtectedPageHandler::ProtectedPageHandler(const userver::components::ComponentConfig& config,
                                             const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context) {}

auto ProtectedPageHandler::HandleRequestJsonThrow(const userver::server::http::HttpRequest& request,
                                                  const userver::formats::json::Value&,
                                                  userver::server::request::RequestContext&) const
    -> userver::formats::json::Value {
  middleware::AttachRequestTags(const_cast<userver::server::http::HttpRequest&>(request));
  spdlog::info("Protected smoke route accessed");

  userver::formats::json::ValueBuilder result;
  result["message"] = "Protected route reached (Phase 5 stub)";
  return dto::MakeSuccessEnvelopeJson(dto::kApiVersion, result.ExtractValue());
}

}  // namespace cppwiki::server::handlers
