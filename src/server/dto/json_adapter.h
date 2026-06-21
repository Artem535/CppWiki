#ifndef CPPWIKI_SRC_SERVER_DTO_JSON_ADAPTER_H_
#define CPPWIKI_SRC_SERVER_DTO_JSON_ADAPTER_H_

#include <rfl/json/read.hpp>
#include <rfl/json/write.hpp>

#include <optional>

#include <userver/formats/json.hpp>

namespace cppwiki::server::dto {

template <typename T>
auto ParseJsonBody(const userver::formats::json::Value& request_body) -> std::optional<T> {
  auto parsed = rfl::json::read<T>(userver::formats::json::ToString(request_body));
  if (!parsed) {
    return std::nullopt;
  }
  return parsed.value();
}

template <typename T>
auto ToJsonValue(const T& value) -> userver::formats::json::Value {
  return userver::formats::json::FromString(rfl::json::write(value));
}

}  // namespace cppwiki::server::dto

#endif  // CPPWIKI_SRC_SERVER_DTO_JSON_ADAPTER_H_
