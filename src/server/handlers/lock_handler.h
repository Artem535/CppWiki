#ifndef CPPWIKI_SRC_SERVER_HANDLERS_LOCK_HANDLER_H_
#define CPPWIKI_SRC_SERVER_HANDLERS_LOCK_HANDLER_H_

#include <string>

#include <userver/server/handlers/http_handler_json_base.hpp>

#include "server/service/lock_service.h"

namespace cppwiki::server::handlers {

class LockHandler final : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-locks";

  LockHandler(const userver::components::ComponentConfig& config,
              const userver::components::ComponentContext& context);

  [[nodiscard]] auto HandleRequestJsonThrow(
      const userver::server::http::HttpRequest& request,
      const userver::formats::json::Value& request_body,
      userver::server::request::RequestContext& context) const
      -> userver::formats::json::Value override;

 private:
  mutable service::LockService lock_service_;

  [[nodiscard]] static auto ExtractOwner(const userver::formats::json::Value& request_body,
                                         const userver::server::http::HttpRequest& request,
                                         const userver::server::request::RequestContext& context)
      -> std::string;
};

}  // namespace cppwiki::server::handlers

#endif  // CPPWIKI_SRC_SERVER_HANDLERS_LOCK_HANDLER_H_
