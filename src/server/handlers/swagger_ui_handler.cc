#include "server/handlers/swagger_ui_handler.h"

#include <userver/server/http/http_response.hpp>

#include "server/middleware/logging_middleware.h"

namespace cppwiki::server::handlers {

namespace {

constexpr std::string_view kSwaggerUiHtml = R"html(<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>CppWiki Swagger UI</title>
    <link rel="stylesheet" href="https://unpkg.com/swagger-ui-dist@5/swagger-ui.css">
    <style>
      body { margin: 0; background: #faf7f0; }
      .topbar { display: none; }
    </style>
  </head>
  <body>
    <div id="swagger-ui"></div>
    <script src="https://unpkg.com/swagger-ui-dist@5/swagger-ui-bundle.js"></script>
    <script>
      window.ui = SwaggerUIBundle({
        url: "/api/v1/openapi.json",
        dom_id: "#swagger-ui",
        deepLinking: true,
        presets: [SwaggerUIBundle.presets.apis]
      });
    </script>
  </body>
</html>
)html";

}  // namespace

SwaggerUiHandler::SwaggerUiHandler(const userver::components::ComponentConfig& config,
                                   const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context) {}

auto SwaggerUiHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request,
                                          userver::server::request::RequestContext&) const
    -> std::string {
  auto& response = request.GetHttpResponse();
  ApplyHeaders(response);
  middleware::AttachRequestTags(const_cast<userver::server::http::HttpRequest&>(request));
  return std::string{kSwaggerUiHtml};
}

void SwaggerUiHandler::ApplyHeaders(userver::server::http::HttpResponse& response) {
  response.SetHeader(std::string_view{"Content-Type"}, "text/html; charset=utf-8");
  response.SetHeader(std::string_view{"Cache-Control"}, "no-store");
}

}  // namespace cppwiki::server::handlers
