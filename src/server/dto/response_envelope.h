#ifndef CPPWIKI_SRC_SERVER_DTO_RESPONSE_ENVELOPE_H_
#define CPPWIKI_SRC_SERVER_DTO_RESPONSE_ENVELOPE_H_

#include <rfl/Rename.hpp>
#include <rfl/json/write.hpp>

#include <string>
#include <utility>

#include <userver/formats/json.hpp>

namespace cppwiki::server::dto {

constexpr int kApiVersion = 1;

struct ErrorDto final {
  std::string code;
  std::string message;
};

template <typename T>
struct SuccessEnvelope final {
  rfl::Rename<"apiVersion", int> api_version{kApiVersion};
  bool ok = true;
  T result;
};

struct ErrorEnvelope final {
  rfl::Rename<"apiVersion", int> api_version{kApiVersion};
  bool ok = false;
  ErrorDto error;
};

template <typename T>
auto ToJsonValue(const T& value) -> userver::formats::json::Value {
  return userver::formats::json::FromString(rfl::json::write(value));
}

template <typename T>
auto MakeSuccessEnvelope(int api_version, T result) -> SuccessEnvelope<T> {
  return SuccessEnvelope<T>{
      .api_version = api_version,
      .ok = true,
      .result = std::move(result),
  };
}

auto MakeErrorEnvelope(int api_version, ErrorDto error) -> ErrorEnvelope;

template <typename T>
auto MakeSuccessEnvelopeJson(int api_version, T result) -> userver::formats::json::Value {
  return ToJsonValue(MakeSuccessEnvelope(api_version, std::move(result)));
}

inline auto MakeErrorEnvelopeJson(int api_version, ErrorDto error) -> userver::formats::json::Value {
  return ToJsonValue(MakeErrorEnvelope(api_version, std::move(error)));
}

}  // namespace cppwiki::server::dto

#endif  // CPPWIKI_SRC_SERVER_DTO_RESPONSE_ENVELOPE_H_
