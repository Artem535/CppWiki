#ifndef CPPWIKI_TESTS_AGENT_FAKE_AGENT_TOOL_CONFIRMATION_PROVIDER_H_
#define CPPWIKI_TESTS_AGENT_FAKE_AGENT_TOOL_CONFIRMATION_PROVIDER_H_

#include <deque>
#include <functional>
#include <utility>
#include <vector>

#include "agent/agent_tool_confirmation_provider.h"

namespace cppwiki::agent::testing {

// A scripted confirmation double that returns queued decisions synchronously and records each
// request so tests can assert exactly what the user would have been shown.
class FakeAgentToolConfirmationProvider final : public AgentToolConfirmationProvider {
 public:
  void QueueDecision(bool approved) { queued_decisions_.push_back(approved); }

  void RequestConfirmation(ToolCallConfirmationRequest request,
                           std::function<void(bool)> on_decided) override {
    received_requests_.push_back(std::move(request));
    if (queued_decisions_.empty()) {
      on_decided(false);
      return;
    }
    const bool approved = queued_decisions_.front();
    queued_decisions_.pop_front();
    on_decided(approved);
  }

  [[nodiscard]] auto ReceivedRequests() const
      -> const std::vector<ToolCallConfirmationRequest>& {
    return received_requests_;
  }

 private:
  std::deque<bool> queued_decisions_;
  std::vector<ToolCallConfirmationRequest> received_requests_;
};

}  // namespace cppwiki::agent::testing

#endif  // CPPWIKI_TESTS_AGENT_FAKE_AGENT_TOOL_CONFIRMATION_PROVIDER_H_
