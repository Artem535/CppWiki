#include "server/handlers/options_handler.h"

#include <string>

#include <userver/server/http/http_response.hpp>

#include "server/middleware/logging_middleware.h"

namespace cppwiki::server::handlers {

OptionsHandler::OptionsHandler(const userver::components::ComponentConfig& config,
                               const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context) {}

auto OptionsHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request,
                                        userver::server::request::RequestContext&) const
    -> std::string {
  auto& response = request.GetHttpResponse();
  ApplyCorsHeaders(response);
  response.SetStatus(userver::server::http::HttpStatus::kNoContent);
  middleware::AttachRequestTags(const_cast<userver::server::http::HttpRequest&>(request));
  return {};
}

void OptionsHandler::ApplyCorsHeaders(userver::server::http::HttpResponse& response) {
  response.SetHeader(std::string_view{"Access-Control-Allow-Origin"}, "*");
  response.SetHeader(std::string_view{"Access-Control-Allow-Methods"},
                     "GET, POST, PUT, DELETE, OPTIONS");
  response.SetHeader(std::string_view{"Access-Control-Allow-Headers"},
                     "Content-Type, Authorization");
}

}  // namespace cppwiki::server::handlers
