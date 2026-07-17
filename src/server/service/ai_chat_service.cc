#include "server/service/ai_chat_service.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <stdexcept>
#include <string>

#include <userver/formats/json.hpp>

namespace cppwiki::server::service {

namespace {

// How much of a raw provider response body to log when it can't be parsed
// as expected. Kept small: enough to diagnose an unexpected shape without
// flooding logs or risking large payloads (the provider response may in
// principle echo back the prompt/context).
constexpr std::size_t kRawBodyLogPreviewLength = 500;

auto TruncatedPreview(const std::string& text, std::size_t max_length) -> std::string {
  if (text.size() <= max_length) {
    return text;
  }
  return text.substr(0, max_length) + "...(truncated)";
}

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

}  // namespace

auto ExtractCompletionText(const userver::formats::json::Value& response) -> std::string {
  const auto choices = response["choices"];
  if (choices.IsMissing() || !choices.IsArray() || choices.GetSize() == 0) {
    throw std::runtime_error("AI provider response did not contain any choices");
  }

  const auto content = choices[0]["message"]["content"];
  if (content.IsString()) {
    return content.As<std::string>();
  }

  // Some OpenAI-compatible backends (notably vision-capable models such as
  // MiMo-VL) reply with a content-parts array instead of a plain string,
  // e.g. `[{"type": "text", "text": "..."}]`, even for text-only replies.
  // Concatenate every part's "text" field, in order, when present.
  if (content.IsArray()) {
    std::string joined;
    for (const auto& part : content) {
      const auto text_field = part["text"];
      if (text_field.IsString()) {
        joined += text_field.As<std::string>();
      }
    }
    if (!joined.empty()) {
      return joined;
    }
    throw std::runtime_error(
        "AI provider response message content was a parts array with no text parts");
  }

  throw std::runtime_error("AI provider response message content was not a string or array");
}

AiChatService::AiChatService(userver::clients::http::Client& http_client, AiProviderConfig config)
    : http_client_(http_client), config_(std::move(config)) {}

auto AiChatService::IsConfigured() const -> bool {
  return config_.enabled && !config_.base_url.empty() && !config_.api_key.empty();
}

auto AiChatService::Complete(const dto::AiChatRequestDto& request) const -> dto::AiChatResultDto {
  if (!IsConfigured()) {
    throw std::runtime_error("AI backend is not configured on the server");
  }

  spdlog::info(
      "AI chat request: mode={} prompt_length={} context_length={}",
      request.mode.value_or("rewrite"), request.prompt.size(),
      request.context ? request.context->size() : 0);

  const auto body = BuildRequestBody(config_, request);

  const auto started_at = std::chrono::steady_clock::now();
  auto response = http_client_.CreateRequest()
                      .post(config_.base_url)
                      .headers({{"Content-Type", "application/json"},
                                {"Authorization", "Bearer " + config_.api_key}})
                      .data(body)
                      .timeout(std::chrono::seconds(config_.timeout_seconds))
                      .SetDestinationMetricName("ai-provider")
                      .perform();
  const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - started_at)
                              .count();

  spdlog::info("AI provider response: status={} elapsed_ms={} body_length={}",
              static_cast<int>(response->status_code()), elapsed_ms, response->body().size());

  if (!response->IsOk()) {
    spdlog::error("AI provider returned non-OK status={} body_preview=\"{}\"",
                  static_cast<int>(response->status_code()),
                  TruncatedPreview(response->body(), kRawBodyLogPreviewLength));
    throw std::runtime_error("AI provider returned HTTP " +
                             std::to_string(static_cast<int>(response->status_code())));
  }

  const auto parsed = userver::formats::json::FromString(response->body());
  try {
    auto text = ExtractCompletionText(parsed);
    spdlog::info("AI chat request completed: text_length={}", text.size());
    return dto::AiChatResultDto{.text = std::move(text)};
  } catch (const std::exception& exception) {
    spdlog::error(
        "AI chat response parsing failed: {} raw_body_preview=\"{}\"", exception.what(),
        TruncatedPreview(response->body(), kRawBodyLogPreviewLength));
    throw;
  }
}

}  // namespace cppwiki::server::service
