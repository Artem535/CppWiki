#ifndef CPPWIKI_SRC_SERVER_ASSETS_OPENAPI_SPEC_H_
#define CPPWIKI_SRC_SERVER_ASSETS_OPENAPI_SPEC_H_

#include <string_view>

namespace cppwiki::server::assets {

inline constexpr std::string_view kOpenApiJson = R"json({
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

}  // namespace cppwiki::server::assets

#endif  // CPPWIKI_SRC_SERVER_ASSETS_OPENAPI_SPEC_H_
