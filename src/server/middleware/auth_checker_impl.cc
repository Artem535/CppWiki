#include "server/middleware/auth_checker_impl.h"

#include <spdlog/spdlog.h>

namespace cppwiki::server::middleware {

auto AuthCheckerImpl::CheckAuth(const userver::server::http::HttpRequest& request,
                                userver::server::request::RequestContext&) const
    -> userver::server::handlers::auth::AuthCheckResult {
  spdlog::debug("Auth check rejected for {} {}", request.GetMethodStr(), request.GetUrl());
  return userver::server::handlers::auth::AuthCheckResult{
      userver::server::handlers::auth::AuthCheckResult::Status::kForbidden,
      "Authentication required (Phase 5 stub)"};
}

}  // namespace cppwiki::server::middleware
