#include "server/handlers/openapi_handler.h"

#include <userver/server/http/http_response.hpp>

#include "server/middleware/logging_middleware.h"

namespace cppwiki::server::handlers {

namespace {

constexpr std::string_view kOpenApiJson = R"json({
  "openapi": "3.0.3",
  "info": {
    "title": "CppWiki Server API",
    "version": "0.1.0",
    "description": "CppWiki local/backend service API."
  },
  "paths": {
    "/api/v1/health": {
      "get": {
        "summary": "Health check",
        "responses": {
          "200": { "description": "Service is healthy" }
        }
      }
    },
    "/api/v1/locks/{document_id}": {
      "get": {
        "summary": "Get document lock",
        "parameters": [
          { "name": "document_id", "in": "path", "required": true, "schema": { "type": "string" } }
        ],
        "responses": {
          "200": { "description": "Lock state returned" },
          "401": { "description": "Unauthorized" }
        }
      },
      "post": {
        "summary": "Acquire document lock",
        "parameters": [
          { "name": "document_id", "in": "path", "required": true, "schema": { "type": "string" } }
        ],
        "responses": {
          "200": { "description": "Lock acquired" },
          "401": { "description": "Unauthorized" }
        }
      },
      "put": {
        "summary": "Heartbeat document lock",
        "parameters": [
          { "name": "document_id", "in": "path", "required": true, "schema": { "type": "string" } }
        ],
        "responses": {
          "200": { "description": "Lock heartbeat accepted" },
          "401": { "description": "Unauthorized" }
        }
      },
      "delete": {
        "summary": "Release document lock",
        "parameters": [
          { "name": "document_id", "in": "path", "required": true, "schema": { "type": "string" } }
        ],
        "responses": {
          "200": { "description": "Lock released" },
          "401": { "description": "Unauthorized" }
        }
      }
    },
    "/api/v1/presence/{workspace_id}": {
      "get": {
        "summary": "Get workspace presence",
        "parameters": [
          { "name": "workspace_id", "in": "path", "required": true, "schema": { "type": "string" } }
        ],
        "responses": {
          "200": { "description": "Presence state returned" },
          "401": { "description": "Unauthorized" }
        }
      },
      "post": {
        "summary": "Update workspace presence",
        "parameters": [
          { "name": "workspace_id", "in": "path", "required": true, "schema": { "type": "string" } }
        ],
        "responses": {
          "200": { "description": "Presence updated" },
          "401": { "description": "Unauthorized" }
        }
      }
    },
    "/api/v1/protected": {
      "get": {
        "summary": "Protected smoke endpoint",
        "responses": {
          "200": { "description": "Protected route reached" },
          "401": { "description": "Unauthorized" }
        }
      }
    }
  }
})json";

}  // namespace

OpenApiHandler::OpenApiHandler(const userver::components::ComponentConfig& config,
                               const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context) {}

auto OpenApiHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request,
                                        userver::server::request::RequestContext&) const
    -> std::string {
  auto& response = request.GetHttpResponse();
  ApplyHeaders(response);
  middleware::AttachRequestTags(const_cast<userver::server::http::HttpRequest&>(request));
  return std::string{kOpenApiJson};
}

void OpenApiHandler::ApplyHeaders(userver::server::http::HttpResponse& response) {
  response.SetHeader(std::string_view{"Content-Type"}, "application/json; charset=utf-8");
  response.SetHeader(std::string_view{"Access-Control-Allow-Origin"}, "*");
}

}  // namespace cppwiki::server::handlers
