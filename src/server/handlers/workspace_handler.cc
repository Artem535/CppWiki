#include "server/handlers/workspace_handler.h"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <stdexcept>

#include <userver/components/component_context.hpp>

#include "server/dto/json_adapter.h"
#include "server/dto/response_envelope.h"
#include "server/dto/workspace_response.h"
#include "server/handlers/handler_utils.h"

namespace cppwiki::server::handlers {

namespace {

constexpr std::string_view kAdminRole = "wiki.admin";

auto RequirePrincipal(userver::server::request::RequestContext& context)
    -> const middleware::JwtPrincipal& {
  const auto* principal =
      context.GetDataOptional<middleware::JwtPrincipal>(middleware::kJwtPrincipalContextKey);
  if (principal == nullptr) {
    throw std::runtime_error("JWT principal missing from workspace request context");
  }
  return *principal;
}

}  // namespace

WorkspaceHandler::WorkspaceHandler(const userver::components::ComponentConfig& config,
                                   const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      sync_config_(
          const_cast<components::SyncBootstrapComponent&>(
              context.FindComponent<components::SyncBootstrapComponent>())) {}

auto WorkspaceHandler::HandleRequestJsonThrow(const userver::server::http::HttpRequest& request,
                                              const userver::formats::json::Value& request_body,
                                              userver::server::request::RequestContext& context) const
    -> userver::formats::json::Value {
  PrepareJsonResponse(request, "GET, POST, OPTIONS");
  const auto& principal = RequirePrincipal(context);

  if (request.GetMethod() == userver::server::http::HttpMethod::kGet) {
    dto::WorkspaceListResultDto result;
    for (const auto& workspace_id : sync_config_.ListWorkspaces()) {
      result.workspaces.push_back(dto::WorkspaceDto{.id = workspace_id});
    }
    return dto::MakeSuccessEnvelopeJson(dto::kApiVersion, std::move(result));
  }

  if (request.GetMethod() != userver::server::http::HttpMethod::kPost) {
    request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kMethodNotAllowed);
    return dto::MakeErrorEnvelopeJson(
        dto::kApiVersion,
        dto::ErrorDto{.code = "method_not_allowed", .message = "Method not allowed"});
  }

  if (!IsAdmin(principal)) {
    request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kForbidden);
    return dto::MakeErrorEnvelopeJson(
        dto::kApiVersion,
        dto::ErrorDto{.code = "forbidden", .message = "Only workspace admins can create workspaces"});
  }

  const auto parsed = dto::ParseJsonBody<dto::WorkspaceCreateRequestDto>(request_body);
  if (!parsed) {
    request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kBadRequest);
    return dto::MakeErrorEnvelopeJson(
        dto::kApiVersion,
        dto::ErrorDto{.code = "invalid_request", .message = "Expected JSON body with workspace id"});
  }

  const auto workspace_id = NormalizeWorkspaceId(parsed->id);
  if (workspace_id.empty()) {
    request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kBadRequest);
    return dto::MakeErrorEnvelopeJson(
        dto::kApiVersion,
        dto::ErrorDto{.code = "invalid_workspace_id",
                       .message = "Workspace id must use [A-Za-z0-9._-] characters"});
  }

  const auto add_result = sync_config_.AddWorkspace(workspace_id);
  if (add_result == components::SyncBootstrapComponent::AddWorkspaceResult::kAlreadyExists) {
    request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kConflict);
    return dto::MakeErrorEnvelopeJson(
        dto::kApiVersion,
        dto::ErrorDto{.code = "workspace_exists", .message = "Workspace already exists"});
  }
  if (add_result == components::SyncBootstrapComponent::AddWorkspaceResult::kPersistFailed) {
    request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kInternalServerError);
    return dto::MakeErrorEnvelopeJson(
        dto::kApiVersion,
        dto::ErrorDto{.code = "workspace_persist_failed",
                       .message = "Workspace registry could not be persisted"});
  }

  request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kCreated);
  return dto::MakeSuccessEnvelopeJson(
      dto::kApiVersion,
      dto::WorkspaceCreateResultDto{
          .workspace = dto::WorkspaceDto{.id = workspace_id},
          .created = true,
      });
}

auto WorkspaceHandler::IsAdmin(const middleware::JwtPrincipal& principal) -> bool {
  return std::ranges::find(principal.roles, std::string(kAdminRole)) != principal.roles.end();
}

auto WorkspaceHandler::NormalizeWorkspaceId(std::string workspace_id) -> std::string {
  workspace_id.erase(
      std::remove_if(workspace_id.begin(), workspace_id.end(),
                     [](unsigned char ch) { return std::isspace(ch) != 0; }),
      workspace_id.end());
  if (workspace_id.empty()) {
    return {};
  }

  const auto valid_char = [](unsigned char ch) {
    return std::isalnum(ch) != 0 || ch == '.' || ch == '-' || ch == '_';
  };
  if (!std::ranges::all_of(workspace_id, valid_char)) {
    return {};
  }

  return workspace_id;
}

}  // namespace cppwiki::server::handlers
