#ifndef CPPWIKI_SRC_SERVER_MIDDLEWARE_AUTH_CHECKER_IMPL_H_
#define CPPWIKI_SRC_SERVER_MIDDLEWARE_AUTH_CHECKER_IMPL_H_

#include <userver/server/handlers/auth/auth_checker_base.hpp>
#include <userver/server/http/http_request.hpp>

namespace cppwiki::server::middleware {

class AuthCheckerImpl final : public userver::server::handlers::auth::AuthCheckerBase {
 public:
  [[nodiscard]] bool SupportsUserAuth() const noexcept override { return false; }

  [[nodiscard]] userver::server::handlers::auth::AuthCheckResult CheckAuth(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& context) const override;
};

}  // namespace cppwiki::server::middleware

#endif  // CPPWIKI_SRC_SERVER_MIDDLEWARE_AUTH_CHECKER_IMPL_H_
