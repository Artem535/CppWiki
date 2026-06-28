#ifndef CPPWIKI_SRC_SERVER_MIDDLEWARE_AUTH_CHECKER_IMPL_H_
#define CPPWIKI_SRC_SERVER_MIDDLEWARE_AUTH_CHECKER_IMPL_H_

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <userver/clients/http/client.hpp>
#include <userver/server/handlers/auth/auth_checker_base.hpp>
#include <userver/server/handlers/auth/handler_auth_config.hpp>
#include <userver/server/http/http_request.hpp>

namespace cppwiki::server::middleware {

struct JwtAuthConfig final {
  std::string issuer;
  std::string audience;
  std::string jwks_url;

  [[nodiscard]] auto IsConfigured() const -> bool {
    return !issuer.empty() && !audience.empty() && !jwks_url.empty();
  }
  [[nodiscard]] static auto FromHandlerAuthConfig(
      const userver::server::handlers::auth::HandlerAuthConfig& config) -> JwtAuthConfig;
};

struct JwtPrincipal final {
  std::string subject;
  std::string preferred_username;
  std::string email;
  std::string issuer;
  std::vector<std::string> roles;
  std::vector<std::string> groups;
};

inline constexpr std::string_view kJwtPrincipalContextKey = "cppwiki.jwt-principal";

class AuthCheckerImpl final : public userver::server::handlers::auth::AuthCheckerBase {
 public:
  AuthCheckerImpl(userver::clients::http::Client& http_client, JwtAuthConfig config);

  [[nodiscard]] bool SupportsUserAuth() const noexcept override { return false; }

  [[nodiscard]] userver::server::handlers::auth::AuthCheckResult CheckAuth(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& context) const override;

 private:
  struct JwksCacheEntry final {
    std::string body;
    std::chrono::steady_clock::time_point fetched_at;
  };

  [[nodiscard]] auto GetJwksBody() const -> std::string;
  [[nodiscard]] auto VerifyBearerToken(std::string_view bearer_token) const
      -> std::optional<JwtPrincipal>;

  userver::clients::http::Client& http_client_;
  JwtAuthConfig config_;
  mutable std::mutex cache_mutex_;
  mutable std::optional<JwksCacheEntry> jwks_cache_;
};

}  // namespace cppwiki::server::middleware

#endif  // CPPWIKI_SRC_SERVER_MIDDLEWARE_AUTH_CHECKER_IMPL_H_
