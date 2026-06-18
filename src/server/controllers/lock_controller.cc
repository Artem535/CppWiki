#include "server/controllers/lock_controller.h"

#include "server/dto/api_response.h"
#include "server/dto/lock_responses.h"
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

void LockController::AcquireOrRefresh(
    const drogon::HttpRequestPtr& request,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& page_id) {
  logging::ServerLogger()->info("Lock acquire/refresh requested for page {}", page_id);

  dto::LockActionResponse payload{
      .page_id = page_id,
      .acquired = true,
      .released = false,
      .owner_user_id = CurrentUserIdFromAttributes(request),
      .expires_in_seconds = 30,
  };

  auto response = drogon::HttpResponse::newHttpResponse();
  response->setStatusCode(drogon::k200OK);
  response->setContentTypeCode(drogon::CT_APPLICATION_JSON);
  response->setBody(dto::SuccessJson(payload));
  callback(response);
}

void LockController::Release(
    const drogon::HttpRequestPtr& request,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& page_id) {
  (void)request;

  logging::ServerLogger()->info("Lock release requested for page {}", page_id);

  dto::LockActionResponse payload{
      .page_id = page_id,
      .acquired = false,
      .released = true,
      .owner_user_id = {},
      .expires_in_seconds = 0,
  };

  auto response = drogon::HttpResponse::newHttpResponse();
  response->setStatusCode(drogon::k200OK);
  response->setContentTypeCode(drogon::CT_APPLICATION_JSON);
  response->setBody(dto::SuccessJson(payload));
  callback(response);
}

void LockController::GetState(
    const drogon::HttpRequestPtr& request,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& page_id) {
  (void)request;

  dto::LockStateResponse payload{
      .page_id = page_id,
      .locked = false,
      .owner_user_id = {},
      .acquired_at = {},
      .expires_in_seconds = 0,
  };

  auto response = drogon::HttpResponse::newHttpResponse();
  response->setStatusCode(drogon::k200OK);
  response->setContentTypeCode(drogon::CT_APPLICATION_JSON);
  response->setBody(dto::SuccessJson(payload));
  callback(response);
}

}  // namespace cppwiki::server::controllers
