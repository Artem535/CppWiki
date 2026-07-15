#include "server/handlers/admin_handler.h"

#include <ranges>
#include <stdexcept>

#include <userver/components/component_context.hpp>

#include "server/dto/admin_response.h"
#include "server/dto/response_envelope.h"
#include "server/handlers/handler_utils.h"

namespace cppwiki::server::handlers {

namespace {

constexpr std::string_view kAdminRole = "wiki.admin";

auto RequirePrincipal(userver::server::request::RequestContext& context)
    -> const middleware::JwtPrincipal& {
  const auto* principal =
      context.GetDataOptional<middleware::JwtPrincipal>(middleware::kJwtPrincipalContextKey);
  if (principal == nullptr) {
    throw std::runtime_error("JWT principal missing from admin request context");
  }
  return *principal;
}

}  // namespace

AdminHandler::AdminHandler(const userver::components::ComponentConfig& config,
                           const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      sync_config_(context.FindComponent<components::SyncBootstrapComponent>()) {}

auto AdminHandler::HandleRequestJsonThrow(const userver::server::http::HttpRequest& request,
                                          const userver::formats::json::Value&,
                                          userver::server::request::RequestContext& context) const
    -> userver::formats::json::Value {
  PrepareJsonResponse(request, "GET, OPTIONS");
  const auto& principal = RequirePrincipal(context);

  if (!IsAdmin(principal)) {
    request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kForbidden);
    return dto::MakeErrorEnvelopeJson(
        dto::kApiVersion,
        dto::ErrorDto{.code = "forbidden", .message = "Only workspace admins can access admin sync state"});
  }

  if (request.GetMethod() != userver::server::http::HttpMethod::kGet) {
    request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kMethodNotAllowed);
    return dto::MakeErrorEnvelopeJson(
        dto::kApiVersion,
        dto::ErrorDto{.code = "method_not_allowed", .message = "Method not allowed"});
  }

  const auto& state = sync_config_.GetState();
  return dto::MakeSuccessEnvelopeJson(
      dto::kApiVersion,
      dto::AdminSyncOverviewResultDto{
          .available = state.available,
          .enabled = state.enabled,
          .gateway_url = rfl::Rename<"gatewayUrl", std::string>{state.gateway_url},
          .admin_url = rfl::Rename<"adminUrl", std::string>{state.admin_url},
          .database_name = rfl::Rename<"databaseName", std::string>{state.database_name},
          .status_text = rfl::Rename<"statusText", std::string>{state.status_text},
          .workspaces = sync_config_.ListWorkspaces(),
          .role_channels =
              rfl::Rename<"roleChannels", std::map<std::string, std::vector<std::string>>>{
                  state.role_channels},
          .group_channels =
              rfl::Rename<"groupChannels", std::map<std::string, std::vector<std::string>>>{
                  state.group_channels},
      });
}

auto AdminHandler::IsAdmin(const middleware::JwtPrincipal& principal) -> bool {
  return std::ranges::find(principal.roles, std::string(kAdminRole)) != principal.roles.end();
}

}  // namespace cppwiki::server::handlers
