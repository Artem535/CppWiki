#include "server/dto/presence_response.h"

#include <chrono>
#include <string>
#include <vector>

#include <userver/formats/json/value_builder.hpp>

namespace cppwiki::server::dto {

auto MakePresenceResultJson(const std::string& workspace_id,
                            const std::vector<service::PresenceInfo>& entries)
    -> userver::formats::json::Value {
  userver::formats::json::ValueBuilder builder;
  builder["workspaceId"] = workspace_id;

  userver::formats::json::ValueBuilder users(userver::formats::common::Type::kArray);
  for (const auto& entry : entries) {
    userver::formats::json::ValueBuilder user_builder;
    user_builder["userId"] = entry.user_id;
    user_builder["scope"] = entry.scope;
    user_builder["lastSeenMsAgo"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - entry.last_seen)
            .count();
    users.PushBack(user_builder.ExtractValue());
  }
  builder["users"] = users.ExtractValue();

  return builder.ExtractValue();
}

}  // namespace cppwiki::server::dto
