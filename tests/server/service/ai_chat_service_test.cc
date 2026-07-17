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

}  // namespace

auto main() -> int {
  TestExtractCompletionTextFromPlainStringContent();
  TestExtractCompletionTextFromContentPartsArray();
  TestExtractCompletionTextThrowsWhenChoicesMissing();
  TestExtractCompletionTextThrowsWhenContentPartsHaveNoText();

  spdlog::info("cppwiki_server_ai_chat_service_tests passed");
  return EXIT_SUCCESS;
}
