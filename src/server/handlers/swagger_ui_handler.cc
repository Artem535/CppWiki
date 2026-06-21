#include "server/handlers/swagger_ui_handler.h"

#include "server/assets/swagger_ui_page.h"
#include "server/handlers/handler_utils.h"

namespace cppwiki::server::handlers {

SwaggerUiHandler::SwaggerUiHandler(const userver::components::ComponentConfig& config,
                                   const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context) {}

auto SwaggerUiHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request,
                                          userver::server::request::RequestContext&) const
    -> std::string {
  PrepareHtmlResponse(request);
  return std::string{assets::kSwaggerUiHtml};
}

}  // namespace cppwiki::server::handlers
