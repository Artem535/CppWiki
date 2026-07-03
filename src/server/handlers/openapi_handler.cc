#include "server/handlers/openapi_handler.h"

#include "server/assets/openapi_spec.h"
#include "server/handlers/handler_utils.h"

namespace cppwiki::server::handlers {

OpenApiHandler::OpenApiHandler(const userver::components::ComponentConfig& config,
                               const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context) {}

auto OpenApiHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request,
                                        userver::server::request::RequestContext&) const
    -> std::string {
  PrepareJsonDocumentResponse(request);
  return std::string{assets::kOpenApiJson};
}

}  // namespace cppwiki::server::handlers
