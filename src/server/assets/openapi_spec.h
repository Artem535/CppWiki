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
  "components": {
    "securitySchemes": {
      "bearerAuth": {
        "type": "http",
        "scheme": "bearer",
        "bearerFormat": "JWT",
        "description": "Paste an Authentik access token."
      }
    },
    "schemas": {
      "LockRequest": {
        "type": "object",
        "properties": {
          "owner": { "type": "string", "example": "artem" }
        }
      },
      "PresenceHeartbeatRequest": {
        "type": "object",
        "properties": {
          "userId": { "type": "string", "example": "artem" },
          "scope": { "type": "string", "example": "edit" }
        }
      },
      "SyncConfigResponse": {
        "type": "object",
        "properties": {
          "available": { "type": "boolean" },
          "enabled": { "type": "boolean" },
          "gatewayUrl": { "type": "string", "example": "http://127.0.0.1:4984/cppwiki" },
          "databaseName": { "type": "string", "example": "cppwiki" },
          "auth": {
            "type": "object",
            "properties": {
              "mode": { "type": "string", "example": "oidc_access_token_passthrough" },
              "tokenPassthrough": { "type": "boolean", "example": true }
            }
          },
          "principal": {
            "type": "object",
            "properties": {
              "subject": { "type": "string", "example": "oidc-subject" },
              "preferredUsername": { "type": "string", "example": "akadmin" },
              "email": { "type": "string", "example": "user@example.com" },
              "roles": {
                "type": "array",
                "items": { "type": "string" },
                "example": ["wiki.editor"]
              },
              "groups": {
                "type": "array",
                "items": { "type": "string" },
                "example": ["team.engineering"]
              }
            }
          },
          "channels": {
            "type": "array",
            "items": { "type": "string" },
            "example": ["user:oidc-subject"]
          },
          "statusText": { "type": "string", "example": "Sync bootstrap is ready" }
        }
      }
    }
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
        "security": [{ "bearerAuth": [] }],
        "parameters": [
          { "name": "document_id", "in": "path", "required": true, "schema": { "type": "string" }, "example": "welcome-page" }
        ],
        "responses": {
          "200": { "description": "Lock state returned" },
          "401": { "description": "Missing or invalid bearer token" }
        }
      },
      "post": {
        "summary": "Acquire document lock",
        "security": [{ "bearerAuth": [] }],
        "parameters": [
          { "name": "document_id", "in": "path", "required": true, "schema": { "type": "string" }, "example": "welcome-page" }
        ],
        "requestBody": {
          "required": false,
          "content": {
            "application/json": {
              "schema": { "$ref": "#/components/schemas/LockRequest" }
            }
          }
        },
        "responses": {
          "200": { "description": "Lock acquired" },
          "401": { "description": "Missing or invalid bearer token" }
        }
      },
      "put": {
        "summary": "Heartbeat document lock",
        "security": [{ "bearerAuth": [] }],
        "parameters": [
          { "name": "document_id", "in": "path", "required": true, "schema": { "type": "string" }, "example": "welcome-page" }
        ],
        "requestBody": {
          "required": false,
          "content": {
            "application/json": {
              "schema": { "$ref": "#/components/schemas/LockRequest" }
            }
          }
        },
        "responses": {
          "200": { "description": "Lock heartbeat accepted" },
          "401": { "description": "Missing or invalid bearer token" }
        }
      },
      "delete": {
        "summary": "Release document lock",
        "security": [{ "bearerAuth": [] }],
        "parameters": [
          { "name": "document_id", "in": "path", "required": true, "schema": { "type": "string" }, "example": "welcome-page" },
          { "name": "force", "in": "query", "required": false, "schema": { "type": "boolean" }, "description": "Force release without owner match" }
        ],
        "requestBody": {
          "required": false,
          "content": {
            "application/json": {
              "schema": { "$ref": "#/components/schemas/LockRequest" }
            }
          }
        },
        "responses": {
          "200": { "description": "Lock released" },
          "401": { "description": "Missing or invalid bearer token" }
        }
      }
    },
    "/api/v1/presence/{workspace_id}": {
      "get": {
        "summary": "Get workspace presence",
        "security": [{ "bearerAuth": [] }],
        "parameters": [
          { "name": "workspace_id", "in": "path", "required": true, "schema": { "type": "string" }, "example": "main-workspace" }
        ],
        "responses": {
          "200": { "description": "Presence state returned" },
          "401": { "description": "Missing or invalid bearer token" }
        }
      },
      "post": {
        "summary": "Update workspace presence",
        "security": [{ "bearerAuth": [] }],
        "parameters": [
          { "name": "workspace_id", "in": "path", "required": true, "schema": { "type": "string" }, "example": "main-workspace" }
        ],
        "requestBody": {
          "required": false,
          "content": {
            "application/json": {
              "schema": { "$ref": "#/components/schemas/PresenceHeartbeatRequest" }
            }
          }
        },
        "responses": {
          "200": { "description": "Presence updated" },
          "401": { "description": "Missing or invalid bearer token" }
        }
      }
    },
    "/api/v1/protected": {
      "get": {
        "summary": "Protected JWT smoke endpoint",
        "security": [{ "bearerAuth": [] }],
        "responses": {
          "200": { "description": "Protected route reached" },
          "401": { "description": "Missing or invalid bearer token" }
        }
      }
    },
    "/api/v1/sync/config": {
      "get": {
        "summary": "Get desktop sync bootstrap config",
        "security": [{ "bearerAuth": [] }],
        "responses": {
          "200": {
            "description": "Sync bootstrap returned",
            "content": {
              "application/json": {
                "schema": { "$ref": "#/components/schemas/SyncConfigResponse" }
              }
            }
          },
          "401": { "description": "Missing or invalid bearer token" }
        }
      }
    }
  }
})json";

}  // namespace cppwiki::server::assets

#endif  // CPPWIKI_SRC_SERVER_ASSETS_OPENAPI_SPEC_H_
