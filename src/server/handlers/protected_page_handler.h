#ifndef CPPWIKI_SRC_SERVER_HANDLERS_PROTECTED_PAGE_HANDLER_H_
#define CPPWIKI_SRC_SERVER_HANDLERS_PROTECTED_PAGE_HANDLER_H_

#include <string>

#include <userver/server/handlers/http_handler_json_base.hpp>

namespace cppwiki::server::handlers {

class ProtectedPageHandler final : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-protected-page";

  ProtectedPageHandler(const userver::components::ComponentConfig& config,
                       const userver::components::ComponentContext& context);

  [[nodiscard]] auto HandleRequestJsonThrow(
      const userver::server::http::HttpRequest& request,
      const userver::formats::json::Value& /*request_body*/,
      userver::server::request::RequestContext& context) const
      -> userver::formats::json::Value override;
};

}  // namespace cppwiki::server::handlers

#endif  // CPPWIKI_SRC_SERVER_HANDLERS_PROTECTED_PAGE_HANDLER_H_
