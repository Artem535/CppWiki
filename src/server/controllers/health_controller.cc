#include "server/controllers/health_controller.h"

#include "server/dto/api_response.h"
#include "server/dto/health_response.h"
#include "server/logging/logger.h"

namespace cppwiki::server::controllers {

void HealthController::Health(
    const drogon::HttpRequestPtr& request,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
  (void)request;

  logging::ServerLogger()->info("Health check requested");

  dto::HealthResponse payload{
      .status = "ok",
      .version = "0.1.0",
  };

  auto response = drogon::HttpResponse::newHttpResponse();
  response->setStatusCode(drogon::k200OK);
  response->setContentTypeCode(drogon::CT_APPLICATION_JSON);
  response->setBody(dto::SuccessJson(payload));
  callback(response);
}

}  // namespace cppwiki::server::controllers
