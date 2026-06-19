#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string>
#include <string_view>

#include "server/dto/health_response.h"
#include "server/dto/response_envelope.h"

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto TestHealthResultContainsExpectedFields() -> void {
  const auto result = cppwiki::server::dto::MakeHealthResult();
  const auto json = cppwiki::server::dto::MakeSuccessEnvelopeJson(
      cppwiki::server::dto::kApiVersion, result);
  const auto json_string = userver::formats::json::ToString(json);

  Require(!json_string.empty(), "health json must not be empty");
  Require(json_string.find("\"apiVersion\":1") != std::string::npos,
          "health json must contain apiVersion");
  Require(json_string.find("\"ok\":true") != std::string::npos,
          "health json must contain ok=true");
  Require(json_string.find("\"service\":") != std::string::npos,
          "health json must contain service");
  Require(json_string.find("\"status\":\"ok\"") != std::string::npos,
          "health json must contain status ok");
  Require(json_string.find("\"result\":") != std::string::npos,
          "health json must contain result");
}

auto TestErrorEnvelopeShape() -> void {
  const auto json = cppwiki::server::dto::MakeErrorEnvelopeJson(
      cppwiki::server::dto::kApiVersion,
      cppwiki::server::dto::ErrorDto{.code = "test", .message = "Test message"});
  const auto json_string = userver::formats::json::ToString(json);

  Require(json_string.find("\"ok\":false") != std::string::npos,
          "error json must contain ok=false");
  Require(json_string.find("\"error\":") != std::string::npos,
          "error json must contain error");
  Require(json_string.find("\"code\":\"test\"") != std::string::npos,
          "error json must contain code");
}

}  // namespace

auto main() -> int {
  TestHealthResultContainsExpectedFields();
  TestErrorEnvelopeShape();

  spdlog::info("cppwiki_server_health_handler_tests passed");
  return EXIT_SUCCESS;
}
