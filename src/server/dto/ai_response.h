#ifndef CPPWIKI_SRC_SERVER_DTO_AI_RESPONSE_H_
#define CPPWIKI_SRC_SERVER_DTO_AI_RESPONSE_H_

#include <rfl/Rename.hpp>

#include <optional>
#include <string>

namespace cppwiki::server::dto {

// Request body for POST /api/v1/ai/chat. MVP scope: rewrite/improve a
// selected block, autocomplete/"continue writing" (see ADR-010, CONTEXT.adoc
// "BlockNote AI (MVP scope)"), and "inline" — continuous ghost-text
// completions (issue #59). `mode` is "rewrite", "autocomplete", or "inline";
// the handler does not branch on it beyond selecting the system-prompt
// prefix sent to the provider (see AiChatService::BuildRequestBody).
// `tool_name`/`tool_schema_json` are optional and are set when the caller
// (the desktop bridge, forwarding xl-ai's tool schema — see issue #65) wants
// a structured tool-call response matching a JSON Schema instead of plain
// text. `tool_schema_json` carries the JSON Schema as a raw string rather
// than a typed field: it is caller-defined and opaque to the server, which
// only forwards it to the provider as-is.
struct AiChatRequestDto final {
  std::string prompt;
  std::optional<std::string> context;
  std::optional<std::string> mode;
  rfl::Rename<"toolName", std::optional<std::string>> tool_name{std::nullopt};
  rfl::Rename<"toolSchemaJson", std::optional<std::string>> tool_schema_json{std::nullopt};
};

// `tool_arguments_json` is populated instead of `text` when the request
// carried a tool schema and the provider replied with a structured tool
// call; `text` is populated for the plain-text path. Exactly one of the two
// is expected to be non-empty for any given response.
struct AiChatResultDto final {
  std::string text;
  rfl::Rename<"toolArgumentsJson", std::optional<std::string>> tool_arguments_json{std::nullopt};
};

}  // namespace cppwiki::server::dto

#endif  // CPPWIKI_SRC_SERVER_DTO_AI_RESPONSE_H_
