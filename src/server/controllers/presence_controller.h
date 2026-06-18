#ifndef CPPWIKI_SRC_SERVER_CONTROLLERS_PRESENCE_CONTROLLER_H_
#define CPPWIKI_SRC_SERVER_CONTROLLERS_PRESENCE_CONTROLLER_H_

#include <drogon/HttpController.h>

namespace cppwiki::server::controllers {

class PresenceController : public drogon::HttpController<PresenceController> {
 public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(PresenceController::Heartbeat, "/api/v1/presence/{pageId}", drogon::Post,
                "cppwiki::server::filters::JwtAuthFilter");
  ADD_METHOD_TO(PresenceController::List, "/api/v1/presence/{pageId}", drogon::Get,
                "cppwiki::server::filters::JwtAuthFilter");
  METHOD_LIST_END

  void Heartbeat(const drogon::HttpRequestPtr& request,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                 const std::string& page_id);

  void List(const drogon::HttpRequestPtr& request,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback,
            const std::string& page_id);
};

}  // namespace cppwiki::server::controllers

#endif  // CPPWIKI_SRC_SERVER_CONTROLLERS_PRESENCE_CONTROLLER_H_
