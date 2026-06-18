#ifndef CPPWIKI_SRC_SERVER_HANDLERS_PRESENCE_HANDLER_H_
#define CPPWIKI_SRC_SERVER_HANDLERS_PRESENCE_HANDLER_H_

#include <string>

#include <userver/server/handlers/http_handler_json_base.hpp>

#include "server/service/presence_service.h"

namespace cppwiki::server::handlers {

class PresenceHandler final : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-presence";

  PresenceHandler(const userver::components::ComponentConfig& config,
                  const userver::components::ComponentContext& context);

  [[nodiscard]] auto HandleRequestJsonThrow(
      const userver::server::http::HttpRequest& request,
      const userver::formats::json::Value& request_body,
      userver::server::request::RequestContext& context) const
      -> userver::formats::json::Value override;

 private:
  mutable service::PresenceService presence_service_;

  static void ApplyCorsHeaders(userver::server::http::HttpResponse& response);
};

}  // namespace cppwiki::server::handlers

#endif  // CPPWIKI_SRC_SERVER_HANDLERS_PRESENCE_HANDLER_H_
