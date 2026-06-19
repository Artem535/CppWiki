#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string>
#include <string_view>

#include "server/middleware/auth_checker_impl.h"

namespace {

class FakeHttpRequest : public userver::server::http::HttpRequest {
 public:
  [[nodiscard]] auto GetMethod() const -> userver::server::http::HttpMethod override {
    return userver::server::http::HttpMethod::kGet;
  }
  [[nodiscard]] auto GetUrl() const -> const std::string& override {
    static const std::string kUrl{"/api/v1/locks/doc-1"};
    return kUrl;
  }
  [[nodiscard]] auto GetHttpResponse() -> userver::server::http::HttpResponse& override {
    static userver::server::http::HttpResponse response{};
    return response;
  }
  [[nodiscard]] auto GetHttpResponse() const -> const userver::server::http::HttpResponse& override {
    static userver::server::http::HttpResponse response{};
    return response;
  }
  [[nodiscard]] auto GetHeader(const std::string&) const -> std::string_view override {
    return {};
  }
  [[nodiscard]] auto GetPathArg(const std::string&) const -> std::string_view override { return {}; }
  [[nodiscard]] auto GetArg(const std::string&) const -> std::string_view override { return {}; }
};

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto TestAuthCheckerRejects() -> void {
  cppwiki::server::middleware::AuthCheckerImpl checker;
  FakeHttpRequest request;
  userver::server::request::RequestContext context;
  const auto result = checker.CheckAuth(request, context);
  Require(result.Status() == userver::server::handlers::auth::AuthCheckResult::Status::kAuthFailed,
          "auth checker must reject all requests");
}

}  // namespace

auto main() -> int {
  TestAuthCheckerRejects();

  spdlog::info("cppwiki_server_auth_checker_tests passed");
  return EXIT_SUCCESS;
}
