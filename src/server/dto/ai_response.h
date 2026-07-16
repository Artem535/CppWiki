#ifndef CPPWIKI_SRC_SERVER_DTO_AI_RESPONSE_H_
#define CPPWIKI_SRC_SERVER_DTO_AI_RESPONSE_H_

#include <optional>
#include <string>

namespace cppwiki::server::dto {

// Request body for POST /api/v1/ai/chat. MVP scope only: rewrite/improve a
// selected block, or autocomplete/"continue writing" (see ADR-010,
// CONTEXT.adoc "BlockNote AI (MVP scope)"). `mode` is "rewrite" or
// "autocomplete"; the handler does not branch on it beyond forwarding it as
// prompt context to the provider.
struct AiChatRequestDto final {
  std::string prompt;
  std::optional<std::string> context;
  std::optional<std::string> mode;
};

struct AiChatResultDto final {
  std::string text;
};

}  // namespace cppwiki::server::dto

#endif  // CPPWIKI_SRC_SERVER_DTO_AI_RESPONSE_H_
