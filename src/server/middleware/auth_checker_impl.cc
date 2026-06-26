#include "server/middleware/auth_checker_impl.h"

#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <rfl/json/read.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <userver/clients/http/component.hpp>
#include <userver/crypto/base64.hpp>
#include <userver/crypto/public_key.hpp>
#include <userver/crypto/verifiers.hpp>
#include <userver/formats/json.hpp>

namespace cppwiki::server::middleware {

namespace {

using AuthCheckResult = userver::server::handlers::auth::AuthCheckResult;

constexpr auto kJwksCacheTtl = std::chrono::minutes(5);

struct JwtHeader final {
  std::string alg;
  std::optional<std::string> kid;
};

struct JwkKey final {
  std::optional<std::string> kid;
  std::optional<std::string> kty;
  std::optional<std::string> alg;
  std::optional<std::string> use;
  std::optional<std::string> n;
  std::optional<std::string> e;
};

struct JwksDocument final {
  std::vector<JwkKey> keys;
};

auto MakeAuthFailure(AuthCheckResult::Status status, std::string reason) -> AuthCheckResult {
  return AuthCheckResult{
      .status = status,
      .reason = std::move(reason),
  };
}

auto ParseBearerToken(std::string_view authorization_header) -> std::optional<std::string_view> {
  constexpr std::string_view kPrefix = "Bearer ";
  if (!authorization_header.starts_with(kPrefix)) {
    return std::nullopt;
  }

  const auto token = authorization_header.substr(kPrefix.size());
  if (token.empty()) {
    return std::nullopt;
  }

  return token;
}

auto SplitToken(std::string_view token) -> std::optional<std::array<std::string_view, 3>> {
  const auto first_dot = token.find('.');
  if (first_dot == std::string_view::npos) {
    return std::nullopt;
  }
  const auto second_dot = token.find('.', first_dot + 1);
  if (second_dot == std::string_view::npos || token.find('.', second_dot + 1) != std::string_view::npos) {
    return std::nullopt;
  }

  return std::array<std::string_view, 3>{
      token.substr(0, first_dot),
      token.substr(first_dot + 1, second_dot - first_dot - 1),
      token.substr(second_dot + 1),
  };
}

auto ReadJwtHeader(std::string_view header_segment) -> JwtHeader {
  const auto decoded = userver::crypto::base64::Base64UrlDecode(header_segment);
  const auto parsed = rfl::json::read<JwtHeader>(decoded);
  if (!parsed) {
    throw std::runtime_error("Could not parse JWT header");
  }
  if (parsed.value().alg.empty()) {
    throw std::runtime_error("JWT header does not contain alg");
  }
  return parsed.value();
}

auto AudienceMatches(const userver::formats::json::Value& payload, std::string_view expected_audience)
    -> bool {
  const auto audience = payload["aud"];
  if (audience.IsMissing()) {
    return false;
  }
  if (audience.IsString()) {
    return audience.As<std::string>() == expected_audience;
  }
  if (!audience.IsArray()) {
    return false;
  }

  for (const auto& value : audience) {
    if (value.IsString() && value.As<std::string>() == expected_audience) {
      return true;
    }
  }
  return false;
}

auto ValidateClaims(const userver::formats::json::Value& payload, const JwtAuthConfig& config) -> JwtPrincipal {
  const auto issuer = payload["iss"].As<std::string>();
  if (issuer != config.issuer) {
    throw std::runtime_error("JWT issuer mismatch");
  }
  if (!AudienceMatches(payload, config.audience)) {
    throw std::runtime_error("JWT audience mismatch");
  }

  const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

  const auto expires_at = payload["exp"].As<std::int64_t>();
  if (expires_at < now) {
    throw std::runtime_error("JWT token expired");
  }

  const auto not_before = payload["nbf"];
  if (!not_before.IsMissing() && not_before.As<std::int64_t>() > now) {
    throw std::runtime_error("JWT token not yet valid");
  }

  return JwtPrincipal{
      .subject = payload["sub"].As<std::string>(),
      .preferred_username = payload["preferred_username"].As<std::string>(""),
      .email = payload["email"].As<std::string>(""),
      .issuer = issuer,
  };
}

auto SelectKey(const JwksDocument& jwks, const JwtHeader& header) -> const JwkKey& {
  for (const auto& key : jwks.keys) {
    if (key.kty != std::optional<std::string>{"RSA"} || !key.n || !key.e) {
      continue;
    }
    if (header.kid && key.kid != header.kid) {
      continue;
    }
    if (key.alg && *key.alg != header.alg) {
      continue;
    }
    return key;
  }
  throw std::runtime_error("Matching JWKS key was not found");
}

auto VerifySignature(const JwkKey& jwk, std::string_view signing_input,
                     std::string_view signature_segment) -> void {
  const auto modulus = userver::crypto::base64::Base64UrlDecode(*jwk.n);
  const auto exponent = userver::crypto::base64::Base64UrlDecode(*jwk.e);
  const auto signature = userver::crypto::base64::Base64UrlDecode(signature_segment);

  const auto public_key = userver::crypto::PublicKey::LoadRSAFromComponents(
      userver::crypto::PublicKey::ModulusView{modulus},
      userver::crypto::PublicKey::ExponentView{exponent});
  const userver::crypto::VerifierRs256 verifier(public_key);
  verifier.Verify({signing_input}, signature);
}

}  // namespace

auto JwtAuthConfig::FromHandlerAuthConfig(
    const userver::server::handlers::auth::HandlerAuthConfig& config) -> JwtAuthConfig {
  return JwtAuthConfig{
      .issuer = config["issuer"].As<std::string>(""),
      .audience = config["audience"].As<std::string>(""),
      .jwks_url = config["jwks_url"].As<std::string>(""),
  };
}

AuthCheckerImpl::AuthCheckerImpl(userver::clients::http::Client& http_client, JwtAuthConfig config)
    : http_client_(http_client), config_(std::move(config)) {}

auto AuthCheckerImpl::CheckAuth(const userver::server::http::HttpRequest& request,
                                userver::server::request::RequestContext& context) const
    -> AuthCheckResult {
  if (!config_.IsConfigured()) {
    spdlog::error("JWT auth is not configured for protected route {}", request.GetUrl());
    return MakeAuthFailure(AuthCheckResult::Status::kInternalCheckFailure,
                           "JWT auth is not configured");
  }

  const auto bearer_token = ParseBearerToken(request.GetHeader("Authorization"));
  if (!bearer_token) {
    return MakeAuthFailure(AuthCheckResult::Status::kInvalidToken,
                           "Bearer token is missing");
  }

  try {
    const auto principal = VerifyBearerToken(*bearer_token);
    if (!principal) {
      return MakeAuthFailure(AuthCheckResult::Status::kInvalidToken,
                             "JWT token could not be verified");
    }

    context.SetData(std::string(kJwtPrincipalContextKey), *principal);
    return MakeAuthFailure(AuthCheckResult::Status::kOk, "JWT token verified");
  } catch (const std::exception& exception) {
    spdlog::warn("JWT auth failed for {} {}: {}", request.GetMethodStr(), request.GetUrl(),
                 exception.what());
    return MakeAuthFailure(AuthCheckResult::Status::kInvalidToken, exception.what());
  }
}

auto AuthCheckerImpl::GetJwksBody() const -> std::string {
  {
    std::lock_guard lock(cache_mutex_);
    if (jwks_cache_ &&
        std::chrono::steady_clock::now() - jwks_cache_->fetched_at < kJwksCacheTtl) {
      return jwks_cache_->body;
    }
  }

  const auto response = http_client_.CreateRequest()
                            .get(config_.jwks_url)
                            .timeout(std::chrono::seconds(3))
                            .SetDestinationMetricName("auth-jwks")
                            .perform();
  if (!response->IsOk()) {
    throw std::runtime_error("JWKS endpoint returned HTTP " +
                             std::to_string(static_cast<int>(response->status_code())));
  }

  auto body = response->body();
  {
    std::lock_guard lock(cache_mutex_);
    jwks_cache_ = JwksCacheEntry{
        .body = body,
        .fetched_at = std::chrono::steady_clock::now(),
    };
  }
  return body;
}

auto AuthCheckerImpl::VerifyBearerToken(std::string_view bearer_token) const
    -> std::optional<JwtPrincipal> {
  const auto segments = SplitToken(bearer_token);
  if (!segments) {
    throw std::runtime_error("JWT token format is invalid");
  }

  const auto header = ReadJwtHeader((*segments)[0]);
  if (header.alg != "RS256") {
    throw std::runtime_error("Unsupported JWT algorithm: " + header.alg);
  }

  const auto payload_json = userver::crypto::base64::Base64UrlDecode((*segments)[1]);
  const auto payload = userver::formats::json::FromString(payload_json);

  const auto jwks_parsed = rfl::json::read<JwksDocument>(GetJwksBody());
  if (!jwks_parsed) {
    throw std::runtime_error("Could not parse JWKS payload");
  }

  const auto& jwk = SelectKey(jwks_parsed.value(), header);
  VerifySignature(jwk, bearer_token.substr(0, bearer_token.rfind('.')), (*segments)[2]);
  return ValidateClaims(payload, config_);
}

}  // namespace cppwiki::server::middleware
