#ifndef CPPWIKI_SRC_SERVER_CONTROLLERS_AUTH_CONTROLLER_H_
#define CPPWIKI_SRC_SERVER_CONTROLLERS_AUTH_CONTROLLER_H_

#include <drogon/HttpController.h>

namespace cppwiki::server::controllers {

class AuthController : public drogon::HttpController<AuthController> {
 public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(AuthController::Refresh, "/api/v1/auth/refresh", drogon::Post,
                "cppwiki::server::filters::JwtAuthFilter");
  ADD_METHOD_TO(AuthController::Me, "/api/v1/auth/me", drogon::Get,
                "cppwiki::server::filters::JwtAuthFilter");
  METHOD_LIST_END

  void Refresh(const drogon::HttpRequestPtr& request,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback);

  void Me(const drogon::HttpRequestPtr& request,
          std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

}  // namespace cppwiki::server::controllers

#endif  // CPPWIKI_SRC_SERVER_CONTROLLERS_AUTH_CONTROLLER_H_
