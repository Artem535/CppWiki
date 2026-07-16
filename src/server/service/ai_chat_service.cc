#include "server/service/ai_chat_service.h"

#include <chrono>
#include <stdexcept>
#include <string>

#include <userver/formats/json.hpp>

namespace cppwiki::server::service {

namespace {

auto BuildRequestBody(const AiProviderConfig& config, const dto::AiChatRequestDto& request)
    -> std::string {
  const auto system_prefix = request.mode.value_or("rewrite") == "autocomplete"
                                 ? "Continue writing the following document naturally."
                                 : "Rewrite/improve the following selection as instructed.";

  userver::formats::json::ValueBuilder builder;
  builder["model"] = config.model.empty() ? "gpt-4o-mini" : config.model;

  userver::formats::json::ValueBuilder messages(userver::formats::common::Type::kArray);

  userver::formats::json::ValueBuilder system_message;
  system_message["role"] = "system";
  system_message["content"] = system_prefix;
  messages.PushBack(system_message.ExtractValue());

  userver::formats::json::ValueBuilder user_message;
  user_message["role"] = "user";
  user_message["content"] =
      (request.context ? *request.context + "\n\n" : std::string{}) + request.prompt;
  messages.PushBack(user_message.ExtractValue());

  builder["messages"] = messages.ExtractValue();
  return userver::formats::json::ToString(builder.ExtractValue());
}

auto ExtractCompletionText(const userver::formats::json::Value& response) -> std::string {
  const auto choices = response["choices"];
  if (choices.IsMissing() || !choices.IsArray() || choices.GetSize() == 0) {
    throw std::runtime_error("AI provider response did not contain any choices");
  }
  return choices[0]["message"]["content"].As<std::string>();
}

}  // namespace

AiChatService::AiChatService(userver::clients::http::Client& http_client, AiProviderConfig config)
    : http_client_(http_client), config_(std::move(config)) {}

auto AiChatService::IsConfigured() const -> bool {
  return config_.enabled && !config_.base_url.empty() && !config_.api_key.empty();
}

auto AiChatService::Complete(const dto::AiChatRequestDto& request) const -> dto::AiChatResultDto {
  if (!IsConfigured()) {
    throw std::runtime_error("AI backend is not configured on the server");
  }

  const auto body = BuildRequestBody(config_, request);

  auto response = http_client_.CreateRequest()
                      .post(config_.base_url)
                      .headers({{"Content-Type", "application/json"},
                                {"Authorization", "Bearer " + config_.api_key}})
                      .data(body)
                      .timeout(std::chrono::seconds(30))
                      .SetDestinationMetricName("ai-provider")
                      .perform();

  if (!response->IsOk()) {
    throw std::runtime_error("AI provider returned HTTP " +
                             std::to_string(static_cast<int>(response->status_code())));
  }

  const auto parsed = userver::formats::json::FromString(response->body());
  return dto::AiChatResultDto{.text = ExtractCompletionText(parsed)};
}

}  // namespace cppwiki::server::service
