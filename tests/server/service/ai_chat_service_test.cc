#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string>
#include <string_view>

#include <userver/formats/json.hpp>

#include "server/service/ai_chat_service.h"

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto TestExtractCompletionTextFromPlainStringContent() -> void {
  const auto response = userver::formats::json::FromString(R"({
    "choices": [
      {"message": {"role": "assistant", "content": "Hello, world."}}
    ]
  })");

  const auto text = cppwiki::server::service::ExtractCompletionText(response);

  Require(text == "Hello, world.", "plain-string content must be extracted verbatim");
}

// Vision-capable OpenAI-compatible backends (e.g. self-hosted MiMo-VL) often
// reply with a content-parts array even for text-only completions, instead
// of a plain string. This must not be silently dropped as empty text.
auto TestExtractCompletionTextFromContentPartsArray() -> void {
  const auto response = userver::formats::json::FromString(R"({
    "choices": [
      {"message": {"role": "assistant", "content": [
        {"type": "text", "text": "Hello, "},
        {"type": "text", "text": "world."}
      ]}}
    ]
  })");

  const auto text = cppwiki::server::service::ExtractCompletionText(response);

  Require(text == "Hello, world.", "content-parts array text parts must be concatenated");
}

auto TestExtractCompletionTextThrowsWhenChoicesMissing() -> void {
  const auto response = userver::formats::json::FromString(R"({"choices": []})");

  bool threw = false;
  try {
    cppwiki::server::service::ExtractCompletionText(response);
  } catch (const std::exception&) {
    threw = true;
  }

  Require(threw, "empty choices array must throw");
}

auto TestExtractCompletionTextThrowsWhenContentPartsHaveNoText() -> void {
  const auto response = userver::formats::json::FromString(R"({
    "choices": [
      {"message": {"role": "assistant", "content": [
        {"type": "image_url", "image_url": {"url": "https://example.com/x.png"}}
      ]}}
    ]
  })");

  bool threw = false;
  try {
    cppwiki::server::service::ExtractCompletionText(response);
  } catch (const std::exception&) {
    threw = true;
  }

  Require(threw, "content-parts array with no text parts must throw, not return empty text");
}

// Structured tool-call response path (issue #65): the server must be able to
// pull a tool call's JSON-stringified arguments out of an OpenAI-compatible
// tool-calling response, since xl-ai only applies document changes from
// tool-call parts and ignores plain text entirely.
auto TestExtractToolCallArgumentsFromToolCallResponse() -> void {
  const auto response = userver::formats::json::FromString(R"({
    "choices": [
      {"message": {"role": "assistant", "content": null, "tool_calls": [
        {"id": "call_1", "type": "function", "function": {
          "name": "applyDocumentOperations",
          "arguments": "{\"operations\":[{\"type\":\"insert\"}]}"
        }}
      ]}}
    ]
  })");

  const auto arguments = cppwiki::server::service::ExtractToolCallArguments(response);

  Require(arguments == R"({"operations":[{"type":"insert"}]})",
         "tool call arguments string must be extracted verbatim");
}

auto TestExtractToolCallArgumentsThrowsWhenToolCallsMissing() -> void {
  const auto response = userver::formats::json::FromString(R"({
    "choices": [
      {"message": {"role": "assistant", "content": "no tool call here"}}
    ]
  })");

  bool threw = false;
  try {
    cppwiki::server::service::ExtractToolCallArguments(response);
  } catch (const std::exception&) {
    threw = true;
  }

  Require(threw, "missing tool_calls must throw, not silently return empty arguments");
}

auto TestExtractToolCallArgumentsThrowsWhenChoicesEmpty() -> void {
  const auto response = userver::formats::json::FromString(R"({"choices": []})");

  bool threw = false;
  try {
    cppwiki::server::service::ExtractToolCallArguments(response);
  } catch (const std::exception&) {
    threw = true;
  }

  Require(threw, "empty choices array must throw");
}

}  // namespace

auto main() -> int {
  TestExtractCompletionTextFromPlainStringContent();
  TestExtractCompletionTextFromContentPartsArray();
  TestExtractCompletionTextThrowsWhenChoicesMissing();
  TestExtractCompletionTextThrowsWhenContentPartsHaveNoText();
  TestExtractToolCallArgumentsFromToolCallResponse();
  TestExtractToolCallArgumentsThrowsWhenToolCallsMissing();
  TestExtractToolCallArgumentsThrowsWhenChoicesEmpty();

  spdlog::info("cppwiki_server_ai_chat_service_tests passed");
  return EXIT_SUCCESS;
}
