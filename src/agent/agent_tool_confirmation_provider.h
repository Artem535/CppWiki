#ifndef CPPWIKI_SRC_AGENT_AGENT_TOOL_CONFIRMATION_PROVIDER_H_
#define CPPWIKI_SRC_AGENT_AGENT_TOOL_CONFIRMATION_PROVIDER_H_

#include <functional>
#include <string>

namespace cppwiki::agent {

// What the user is asked to approve before one mutating tool call runs. Requests are per call,
// never batched per model turn, so the summary describes exactly one action.
struct ToolCallConfirmationRequest {
  std::string tool_name;
  std::string summary;
};

// Gates a mutating AI Chat (and later Code mode) tool call on an explicit user decision. A
// caller without a provider must fail closed rather than auto-approving a mutation.
class AgentToolConfirmationProvider {
 public:
  virtual ~AgentToolConfirmationProvider() = default;

  // Invokes on_decided exactly once with the user's decision.
  virtual void RequestConfirmation(ToolCallConfirmationRequest request,
                                   std::function<void(bool approved)> on_decided) = 0;
};

}  // namespace cppwiki::agent

#endif  // CPPWIKI_SRC_AGENT_AGENT_TOOL_CONFIRMATION_PROVIDER_H_
