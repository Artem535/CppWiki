#include "server/filters/jwt_auth_filter.h"

#include "server/dto/api_response.h"

namespace cppwiki::server::filters {

void JwtAuthFilter::doFilter(const drogon::HttpRequestPtr& request,
                             drogon::FilterCallback&& fcb,
                             drogon::FilterChainCallback&& fccb) {
  const auto token = ExtractBearerToken(request);
  if (token.empty()) {
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(drogon::k401Unauthorized);
    response->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    response->setBody(
        dto::ErrorJson("unauthorized", "Missing or invalid Authorization header"));
    fcb(response);
    return;
  }

  // Skeleton: attach the raw token as a request attribute for controllers.
  // Future: validate signature, issuer, audience and expiration (ADR-008).
  request->attributes()->insert("jwt_token", token);
  fccb();
}

auto JwtAuthFilter::ExtractBearerToken(const drogon::HttpRequestPtr& request)
    -> std::string {
  const auto& header = request->getHeader(std::string{kAuthorizationHeader});
  if (header.empty()) {
    return {};
  }
  if (header.rfind(std::string{kBearerPrefix}, 0) != 0) {
    return {};
  }
  return header.substr(kBearerPrefix.size());
}

}  // namespace cppwiki::server::filters
