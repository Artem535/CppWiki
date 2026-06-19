#include "server/handlers/protected_page_handler.h"

#include <string>

#include <spdlog/spdlog.h>

#include "server/dto/response_envelope.h"
#include "server/middleware/logging_middleware.h"

namespace cppwiki::server::handlers {

namespace {

struct ProtectedPageResult final {
  std::string message;
};

}  // namespace

ProtectedPageHandler::ProtectedPageHandler(const userver::components::ComponentConfig& config,
                                             const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context) {}

auto ProtectedPageHandler::HandleRequestJsonThrow(const userver::server::http::HttpRequest& request,
                                                  const userver::formats::json::Value&,
                                                  userver::server::request::RequestContext&) const
    -> userver::formats::json::Value {
  middleware::AttachRequestTags(const_cast<userver::server::http::HttpRequest&>(request));
  spdlog::info("Protected smoke route accessed");

  return dto::MakeSuccessEnvelopeJson(
      dto::kApiVersion,
      ProtectedPageResult{.message = "Protected route reached (Phase 5 stub)"});
}

}  // namespace cppwiki::server::handlers
