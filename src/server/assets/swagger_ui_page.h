#ifndef CPPWIKI_SRC_SERVER_ASSETS_SWAGGER_UI_PAGE_H_
#define CPPWIKI_SRC_SERVER_ASSETS_SWAGGER_UI_PAGE_H_

#include <string_view>

namespace cppwiki::server::assets {

inline constexpr std::string_view kSwaggerUiHtml = R"html(<!doctype html>
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
        persistAuthorization: true,
        displayRequestDuration: true,
        presets: [SwaggerUIBundle.presets.apis]
      });
    </script>
  </body>
</html>
)html";

}  // namespace cppwiki::server::assets

#endif  // CPPWIKI_SRC_SERVER_ASSETS_SWAGGER_UI_PAGE_H_
