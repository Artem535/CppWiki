#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string_view>

#include "server/middleware/auth_checker_impl.h"

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto TestJwtAuthConfigRequiresAllFields() -> void {
  using cppwiki::server::middleware::JwtAuthConfig;

  Require(!JwtAuthConfig{.issuer = "", .audience = "a", .jwks_url = "u"}.IsConfigured(),
          "empty issuer must keep auth config disabled");
  Require(!JwtAuthConfig{.issuer = "i", .audience = "", .jwks_url = "u"}.IsConfigured(),
          "empty audience must keep auth config disabled");
  Require(!JwtAuthConfig{.issuer = "i", .audience = "a", .jwks_url = ""}.IsConfigured(),
          "empty jwks url must keep auth config disabled");
  Require(JwtAuthConfig{.issuer = "i", .audience = "a", .jwks_url = "u"}.IsConfigured(),
          "all jwt auth fields should enable auth config");
}

}  // namespace

auto main() -> int {
  TestJwtAuthConfigRequiresAllFields();

  spdlog::info("cppwiki_server_auth_checker_tests passed");
  return EXIT_SUCCESS;
}
