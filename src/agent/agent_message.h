#ifndef CPPWIKI_SRC_AGENT_AGENT_MESSAGE_H_
#define CPPWIKI_SRC_AGENT_AGENT_MESSAGE_H_

#include <optional>
#include <string>
#include <vector>

namespace cppwiki::agent {

enum class AgentMessageRole { kSystem, kUser, kAssistant, kTool };

// The Agent engine's parsed form of one function-calling invocation the model asked for, per
// the "Tool call" glossary entry (doc/modules/ROOT/pages/CONTEXT.adoc). `arguments_json` is
// opaque to the engine -- it is handed to the tool registry verbatim.
struct AgentToolCall {
  std::string id;
  std::string tool_name;
  std::string arguments_json;
};

// One turn in an Agent session, per the "Agent message" glossary entry. An assistant turn may
// carry zero or more tool calls; a tool turn carries the result of exactly one of them, tied
// back via `tool_call_id`.
struct AgentMessage {
  AgentMessageRole role = AgentMessageRole::kUser;
  std::string content;
  std::vector<AgentToolCall> tool_calls;
  std::optional<std::string> tool_call_id;
};

}  // namespace cppwiki::agent

#endif  // CPPWIKI_SRC_AGENT_AGENT_MESSAGE_H_
