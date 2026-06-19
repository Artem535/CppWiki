#include "server/dto/health_response.h"

#include <userver/formats/json/value_builder.hpp>

#include "core/constants.h"

namespace cppwiki::server::dto {

auto MakeHealthResult() -> userver::formats::json::Value {
  userver::formats::json::ValueBuilder builder;
  builder["service"] = std::string(cppwiki::constants::kServerServiceName);
  builder["status"] = "ok";
  return builder.ExtractValue();
}

}  // namespace cppwiki::server::dto
