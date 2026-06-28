#include "server/service/sync_gateway_adapter.h"

#include <algorithm>
#include <map>
#include <set>
#include <string_view>
#include <utility>

namespace cppwiki::server::service {

namespace {

constexpr std::string_view kAuthMode = "oidc_access_token_passthrough";

}

SyncGatewayAdapter::SyncGatewayAdapter(const components::SyncBootstrapState& state) : state_(state) {}

auto SyncGatewayAdapter::BuildBootstrap(const middleware::JwtPrincipal& principal) const
    -> dto::SyncConfigResultDto {
  return dto::SyncConfigResultDto{
      .available = state_.available,
      .enabled = state_.enabled,
      .gateway_url = state_.gateway_url,
      .database_name = state_.database_name,
      .auth =
          dto::SyncAuthConfigDto{
              .mode = std::string(kAuthMode),
              .token_passthrough = true,
          },
      .principal =
          dto::SyncPrincipalDto{
              .subject = principal.subject,
              .preferred_username = principal.preferred_username,
              .email = principal.email,
              .roles = principal.roles,
              .groups = principal.groups,
          },
      .channels = DeriveChannels(principal),
      .status_text = state_.status_text,
  };
}

auto SyncGatewayAdapter::DeriveChannels(const middleware::JwtPrincipal& principal) const
    -> std::vector<std::string> {
  std::set<std::string> unique_channels;

  if (principal.subject.empty()) {
    return {};
  }

  unique_channels.insert(std::string("user:") + principal.subject);

  const auto append_channels =
      [&unique_channels](const auto& mapping, const auto& claims) {
        for (const auto& claim : claims) {
          const auto it = mapping.find(claim);
          if (it == mapping.end()) {
            continue;
          }

          for (const auto& channel : it->second) {
            if (!channel.empty()) {
              unique_channels.insert(channel);
            }
          }
        }
      };

  append_channels(state_.role_channels, principal.roles);
  append_channels(state_.group_channels, principal.groups);

  return {unique_channels.begin(), unique_channels.end()};
}

}  // namespace cppwiki::server::service
