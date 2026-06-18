#ifndef CPPWIKI_SRC_SERVER_HANDLERS_HEALTH_HANDLER_H_
#define CPPWIKI_SRC_SERVER_HANDLERS_HEALTH_HANDLER_H_

#include <string>

#include <userver/server/handlers/http_handler_json_base.hpp>

namespace cppwiki::server::handlers {

class HealthHandler final : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-health";

  HealthHandler(const userver::components::ComponentConfig& config,
                const userver::components::ComponentContext& context);

  [[nodiscard]] auto HandleRequestJsonThrow(
      const userver::server::http::HttpRequest& request,
      const userver::formats::json::Value& /*request_body*/,
      userver::server::request::RequestContext& context) const
      -> userver::formats::json::Value override;

 private:
  static void ApplyCorsHeaders(userver::server::http::HttpResponse& response);
};

}  // namespace cppwiki::server::handlers

#endif  // CPPWIKI_SRC_SERVER_HANDLERS_HEALTH_HANDLER_H_
