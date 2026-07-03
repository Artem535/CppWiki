#include "server/handlers/options_handler.h"

#include <string>

#include <userver/server/http/http_response.hpp>

#include "server/handlers/handler_utils.h"

namespace cppwiki::server::handlers {

OptionsHandler::OptionsHandler(const userver::components::ComponentConfig& config,
                               const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context) {}

auto OptionsHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request,
                                        userver::server::request::RequestContext&) const
    -> std::string {
  PrepareJsonResponse(request, "GET, POST, PUT, DELETE, OPTIONS");
  auto& response = request.GetHttpResponse();
  response.SetStatus(userver::server::http::HttpStatus::kNoContent);
  return {};
}

}  // namespace cppwiki::server::handlers
