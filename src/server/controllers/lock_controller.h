#ifndef CPPWIKI_SRC_SERVER_CONTROLLERS_LOCK_CONTROLLER_H_
#define CPPWIKI_SRC_SERVER_CONTROLLERS_LOCK_CONTROLLER_H_

#include <drogon/HttpController.h>

namespace cppwiki::server::controllers {

class LockController : public drogon::HttpController<LockController> {
 public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(LockController::AcquireOrRefresh, "/api/v1/locks/{pageId}", drogon::Post,
                "cppwiki::server::filters::JwtAuthFilter");
  ADD_METHOD_TO(LockController::Release, "/api/v1/locks/{pageId}", drogon::Delete,
                "cppwiki::server::filters::JwtAuthFilter");
  ADD_METHOD_TO(LockController::GetState, "/api/v1/locks/{pageId}", drogon::Get,
                "cppwiki::server::filters::JwtAuthFilter");
  METHOD_LIST_END

  void AcquireOrRefresh(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& page_id);

  void Release(const drogon::HttpRequestPtr& request,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback,
               const std::string& page_id);

  void GetState(const drogon::HttpRequestPtr& request,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                const std::string& page_id);
};

}  // namespace cppwiki::server::controllers

#endif  // CPPWIKI_SRC_SERVER_CONTROLLERS_LOCK_CONTROLLER_H_
