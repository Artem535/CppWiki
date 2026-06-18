#include "server/controllers/auth_controller.h"

#include "server/dto/api_response.h"
#include "server/dto/auth_responses.h"
#include "server/logging/logger.h"

namespace cppwiki::server::controllers {

namespace {

[[nodiscard]] auto CurrentUserIdFromAttributes(const drogon::HttpRequestPtr& request)
    -> std::string {
  const auto attributes = request->attributes();
  if (!attributes) {
    return "unknown";
  }
  return attributes->get<std::string>("jwt_token");
}

}  // namespace

void AuthController::Refresh(
    const drogon::HttpRequestPtr& request,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
  (void)request;

  logging::ServerLogger()->info("Token refresh requested");

  dto::RefreshResponse payload{
      .access_token = "stub-refreshed-token",
      .token_type = "Bearer",
      .expires_in = 3600,
  };

  auto response = drogon::HttpResponse::newHttpResponse();
  response->setStatusCode(drogon::k200OK);
  response->setContentTypeCode(drogon::CT_APPLICATION_JSON);
  response->setBody(dto::SuccessJson(payload));
  callback(response);
}

void AuthController::Me(
    const drogon::HttpRequestPtr& request,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
  auto response = drogon::HttpResponse::newHttpResponse();
  response->setStatusCode(drogon::k200OK);
  response->setContentTypeCode(drogon::CT_APPLICATION_JSON);

  dto::UserProfileResponse payload{
      .sub = CurrentUserIdFromAttributes(request),
      .email = "user@example.com",
      .display_name = "Stub User",
      .roles = {"wiki.viewer"},
  };
  response->setBody(dto::SuccessJson(payload));
  callback(response);
}

}  // namespace cppwiki::server::controllers
