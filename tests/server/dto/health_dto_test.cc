#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string>
#include <string_view>

#include "core/constants.h"
#include "oatpp/core/base/Environment.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "server/dto/api_dto.h"

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto TestHealthEnvelopeSerialization() -> void {
  auto result = cppwiki::server::dto::HealthStatusDto::createShared();
  result->service = oatpp::String(cppwiki::constants::kServerServiceName.data());
  result->status = oatpp::String("ok");

  auto envelope = cppwiki::server::dto::HealthEnvelopeDto::createShared();
  envelope->apiVersion = cppwiki::constants::kServerApiVersion;
  envelope->ok = true;
  envelope->result = result;

  auto object_mapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
  const std::string json_string = object_mapper->writeToString(envelope);

  Require(!json_string.empty(), "health json must not be empty");
  Require(json_string.find("\"apiVersion\":1") != std::string::npos,
          "health json must contain apiVersion");
  Require(json_string.find("\"ok\":true") != std::string::npos,
          "health json must contain ok=true");
  Require(json_string.find("\"service\":\"cppwiki-server\"") != std::string::npos,
          "health json must contain server service name");
  Require(json_string.find("\"status\":\"ok\"") != std::string::npos,
          "health json must contain status");
}

}  // namespace

auto main() -> int {
  oatpp::base::Environment::init();
  TestHealthEnvelopeSerialization();
  oatpp::base::Environment::destroy();

  spdlog::info("cppwiki_server_health_dto_tests passed");
  return EXIT_SUCCESS;
}
