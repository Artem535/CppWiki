#include "server/handlers/protected_page_handler.h"

#include <stdexcept>
#include <string>

#include <spdlog/spdlog.h>

#include "server/dto/response_envelope.h"
#include "server/middleware/auth_checker_impl.h"
#include "server/middleware/logging_middleware.h"

namespace cppwiki::server::handlers {

namespace {

struct ProtectedPageResult final {
  std::string message;
  std::string subject;
  std::string preferred_username;
  std::string email;
  std::string issuer;
};

}  // namespace

ProtectedPageHandler::ProtectedPageHandler(const userver::components::ComponentConfig& config,
                                             const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context) {}

auto ProtectedPageHandler::HandleRequestJsonThrow(const userver::server::http::HttpRequest& request,
                                                  const userver::formats::json::Value&,
                                                  userver::server::request::RequestContext& context) const
    -> userver::formats::json::Value {
  middleware::AttachRequestTags(const_cast<userver::server::http::HttpRequest&>(request));
  const auto* principal =
      context.GetDataOptional<middleware::JwtPrincipal>(middleware::kJwtPrincipalContextKey);
  if (!principal) {
    throw std::runtime_error("JWT principal missing from protected request context");
  }

  spdlog::info("Protected route accessed by subject={}", principal->subject);

  return dto::MakeSuccessEnvelopeJson(
      dto::kApiVersion,
      ProtectedPageResult{
          .message = "Protected route reached",
          .subject = principal->subject,
          .preferred_username = principal->preferred_username,
          .email = principal->email,
          .issuer = principal->issuer,
      });
}

}  // namespace cppwiki::server::handlers
