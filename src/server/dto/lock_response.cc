#include "server/dto/lock_response.h"

#include <optional>
#include <string>

#include <userver/formats/json/value_builder.hpp>

namespace cppwiki::server::dto {

auto MakeLockResultJson(const LockActionResult& result) -> userver::formats::json::Value {
  userver::formats::json::ValueBuilder builder;
  builder["acquired"] = result.acquired;
  builder["released"] = result.released;
  builder["heartbeat"] = result.heartbeat;
  builder["forceReleased"] = result.force_released;
  if (result.owner) {
    builder["owner"] = *result.owner;
  }
  if (result.document_id) {
    builder["documentId"] = *result.document_id;
  }
  return builder.ExtractValue();
}

auto MakeLockOwnerResultJson(const std::string& document_id,
                             const std::optional<std::string>& owner)
    -> userver::formats::json::Value {
  userver::formats::json::ValueBuilder builder;
  builder["documentId"] = document_id;
  builder["locked"] = owner.has_value();
  if (owner) {
    builder["owner"] = *owner;
  }
  return builder.ExtractValue();
}

}  // namespace cppwiki::server::dto
