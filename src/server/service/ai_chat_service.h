#ifndef CPPWIKI_SRC_SERVER_SERVICE_AI_CHAT_SERVICE_H_
#define CPPWIKI_SRC_SERVER_SERVICE_AI_CHAT_SERVICE_H_

#include <cstdint>
#include <string>

#include <userver/clients/http/client.hpp>
#include <userver/formats/json/value.hpp>

#include "server/dto/ai_response.h"

namespace cppwiki::server::service {

// Extracts the assistant's reply text from an OpenAI-compatible
// `/chat/completions` response body (`choices[0].message.content`). Handles
// both the common plain-string shape and the content-parts-array shape used
// by some OpenAI-compatible backends (in particular vision-capable models,
// which often reply with `content: [{"type": "text", "text": "..."}]` even
// for text-only completions). Throws `std::runtime_error` if `choices` is
// missing/empty, or if `content` is present but not a recognized shape.
// Exposed here (rather than kept file-local) so it can be unit-tested
// without a live provider.
auto ExtractCompletionText(const userver::formats::json::Value& response) -> std::string;

// Extracts a structured tool call's arguments (a JSON string) from an
// OpenAI-compatible `/chat/completions` response body
// (`choices[0].message.tool_calls[0].function.arguments`). Used instead of
// ExtractCompletionText() when the request carried a tool schema (see issue
// #65: xl-ai only applies document changes from tool-call responses, never
// from plain text). Throws `std::runtime_error` if `choices`/`tool_calls` is
// missing/empty or `arguments` is not a string.
auto ExtractToolCallArguments(const userver::formats::json::Value& response) -> std::string;

struct AiProviderConfig final {
  bool enabled = false;
  std::string base_url;
  std::string api_key;
  std::string model;
  // Some OpenAI-compatible backends (e.g. self-hosted vision/reasoning models like MiMo-VL)
  // take much longer per completion than a hosted API like OpenAI's; make this configurable
  // instead of a fixed 30s so a slow model doesn't look like a network failure.
  std::uint32_t timeout_seconds = 30;
};

// Server-mediated AI backend (ADR-012): holds the AI provider's API key here,
// never in the client, and proxies chat requests to an OpenAI-compatible
// `/chat/completions` endpoint. MVP scope only (ADR-010): rewrite/improve and
// autocomplete, no multi-step agent/RAG.
class AiChatService final {
 public:
  AiChatService(userver::clients::http::Client& http_client, AiProviderConfig config);

  [[nodiscard]] auto IsConfigured() const -> bool;
  [[nodiscard]] auto Complete(const dto::AiChatRequestDto& request) const -> dto::AiChatResultDto;

 private:
  userver::clients::http::Client& http_client_;
  AiProviderConfig config_;
};

}  // namespace cppwiki::server::service

#endif  // CPPWIKI_SRC_SERVER_SERVICE_AI_CHAT_SERVICE_H_
