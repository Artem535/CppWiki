#ifndef CPPWIKI_SRC_SERVER_DTO_RESPONSE_ENVELOPE_H_
#define CPPWIKI_SRC_SERVER_DTO_RESPONSE_ENVELOPE_H_

#include <string>

#include <userver/formats/json/value_builder.hpp>

namespace cppwiki::server::dto {

constexpr int kApiVersion = 1;

struct ErrorDto final {
  std::string code;
  std::string message;
};

template <typename T>
struct ResponseEnvelope final {
  bool ok = true;
  T result{};
};

template <typename T>
auto MakeSuccessEnvelopeJson(int api_version, const T& result_json) -> userver::formats::json::Value;

auto MakeErrorEnvelopeJson(int api_version, const ErrorDto& error) -> userver::formats::json::Value;

template <typename T>
auto MakeResult(const T& value) -> userver::formats::json::Value {
  userver::formats::json::ValueBuilder builder;
  builder = value;
  return builder.ExtractValue();
}

}  // namespace cppwiki::server::dto

#endif  // CPPWIKI_SRC_SERVER_DTO_RESPONSE_ENVELOPE_H_
