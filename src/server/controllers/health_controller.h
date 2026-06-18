#ifndef CPPWIKI_SRC_SERVER_CONTROLLERS_HEALTH_CONTROLLER_H_
#define CPPWIKI_SRC_SERVER_CONTROLLERS_HEALTH_CONTROLLER_H_

#include <drogon/HttpController.h>

namespace cppwiki::server::controllers {

class HealthController : public drogon::HttpController<HealthController> {
 public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(HealthController::Health, "/api/v1/health", drogon::Get);
  METHOD_LIST_END

  void Health(const drogon::HttpRequestPtr& request,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

}  // namespace cppwiki::server::controllers

#endif  // CPPWIKI_SRC_SERVER_CONTROLLERS_HEALTH_CONTROLLER_H_
