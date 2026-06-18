#include "server/controllers/presence_controller.h"

#include "server/dto/api_response.h"
#include "server/dto/presence_responses.h"
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

void PresenceController::Heartbeat(
    const drogon::HttpRequestPtr& request,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& page_id) {
  logging::ServerLogger()->info("Presence heartbeat for page {} from user {}",
                                 page_id, CurrentUserIdFromAttributes(request));

  dto::PresenceHeartbeatResponse payload{
      .page_id = page_id,
      .user_id = CurrentUserIdFromAttributes(request),
      .heartbeat_interval_seconds = 10,
  };

  auto response = drogon::HttpResponse::newHttpResponse();
  response->setStatusCode(drogon::k200OK);
  response->setContentTypeCode(drogon::CT_APPLICATION_JSON);
  response->setBody(dto::SuccessJson(payload));
  callback(response);
}

void PresenceController::List(
    const drogon::HttpRequestPtr& request,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& page_id) {
  (void)request;

  dto::PresenceListResponse payload{
      .page_id = page_id,
      .users = {},
  };

  auto response = drogon::HttpResponse::newHttpResponse();
  response->setStatusCode(drogon::k200OK);
  response->setContentTypeCode(drogon::CT_APPLICATION_JSON);
  response->setBody(dto::SuccessJson(payload));
  callback(response);
}

}  // namespace cppwiki::server::controllers
