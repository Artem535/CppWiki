#include "server/dto/response_envelope.h"

namespace cppwiki::server::dto {

auto MakeErrorEnvelopeJson(int api_version, const ErrorDto& error) -> userver::formats::json::Value {
  userver::formats::json::ValueBuilder builder;
  builder["apiVersion"] = api_version;
  builder["ok"] = false;

  userver::formats::json::ValueBuilder error_builder;
  error_builder["code"] = error.code;
  error_builder["message"] = error.message;
  builder["error"] = error_builder.ExtractValue();

  return builder.ExtractValue();
}

template <typename T>
auto MakeSuccessEnvelopeJson(int api_version, const T& result_json) -> userver::formats::json::Value {
  userver::formats::json::ValueBuilder builder;
  builder["apiVersion"] = api_version;
  builder["ok"] = true;
  builder["result"] = result_json;
  return builder.ExtractValue();
}

template auto MakeSuccessEnvelopeJson<userver::formats::json::Value>(
    int, const userver::formats::json::Value&) -> userver::formats::json::Value;

template auto MakeSuccessEnvelopeJson<userver::formats::json::ValueBuilder>(
    int, const userver::formats::json::ValueBuilder&) -> userver::formats::json::Value;

}  // namespace cppwiki::server::dto
