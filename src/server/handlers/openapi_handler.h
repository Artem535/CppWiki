#ifndef CPPWIKI_SRC_SERVER_HANDLERS_OPENAPI_HANDLER_H_
#define CPPWIKI_SRC_SERVER_HANDLERS_OPENAPI_HANDLER_H_

#include <string>

#include <userver/server/handlers/http_handler_base.hpp>

namespace cppwiki::server::handlers {

class OpenApiHandler final : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-openapi";

  OpenApiHandler(const userver::components::ComponentConfig& config,
                 const userver::components::ComponentContext& context);

  [[nodiscard]] auto HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& context) const -> std::string override;

 private:
  static void ApplyHeaders(userver::server::http::HttpResponse& response);
};

}  // namespace cppwiki::server::handlers

#endif  // CPPWIKI_SRC_SERVER_HANDLERS_OPENAPI_HANDLER_H_
