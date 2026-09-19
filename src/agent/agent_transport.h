#ifndef CPPWIKI_SRC_AGENT_AGENT_TRANSPORT_H_
#define CPPWIKI_SRC_AGENT_AGENT_TRANSPORT_H_

#include <functional>
#include <string>
#include <variant>
#include <vector>

#include "agent/agent_message.h"
#include "agent/agent_tool_registry.h"

namespace cppwiki::agent {

// What one round trip needs: the conversation so far, and the tool schemas the model may call.
struct AgentChatRequest {
  std::vector<AgentMessage> messages;
  std::vector<AgentToolSchema> tools;
};

// Either the model's next assistant turn (content and/or tool calls), or an error description.
using AgentChatOutcome = std::variant<AgentMessage, std::string>;

// The Agent engine's abstraction for one chat-completions round trip against an
// OpenAI-compatible endpoint, per the "Agent transport" glossary entry
// (doc/modules/ROOT/pages/CONTEXT.adoc). A real implementation is an HTTP call
// (see QtAgentTransport); unit tests substitute a fake -- no real network calls in tests.
class AgentTransport {
 public:
  virtual ~AgentTransport() = default;

  // Invokes `on_done` exactly once with the outcome. Asynchronous by design (a real
  // implementation is an HTTP call); a fake used in tests may call it synchronously, inline.
  virtual void SendChatRequest(const AgentChatRequest& request,
                               std::function<void(AgentChatOutcome)> on_done) = 0;
};

}  // namespace cppwiki::agent

#endif  // CPPWIKI_SRC_AGENT_AGENT_TRANSPORT_H_
