#ifndef CPPWIKI_SRC_SERVER_FILTERS_JWT_AUTH_FILTER_H_
#define CPPWIKI_SRC_SERVER_FILTERS_JWT_AUTH_FILTER_H_

#include <drogon/HttpFilter.h>

namespace cppwiki::server::filters {

// Placeholder JWT validation filter.
// For the skeleton stage, the filter only checks that an
// "Authorization: Bearer <token>" header is present.
// Real JWKS signature, issuer, audience and expiration validation is deferred
// to the Auth spike (Milestone 6).
class JwtAuthFilter : public drogon::HttpFilter<JwtAuthFilter> {
 public:
  JwtAuthFilter() = default;

  void doFilter(const drogon::HttpRequestPtr& request,
                drogon::FilterCallback&& fcb,
                drogon::FilterChainCallback&& fccb) override;

 private:
  [[nodiscard]] static auto ExtractBearerToken(const drogon::HttpRequestPtr& request)
      -> std::string;

  static constexpr std::string_view kAuthorizationHeader = "Authorization";
  static constexpr std::string_view kBearerPrefix = "Bearer ";
};

}  // namespace cppwiki::server::filters

#endif  // CPPWIKI_SRC_SERVER_FILTERS_JWT_AUTH_FILTER_H_
