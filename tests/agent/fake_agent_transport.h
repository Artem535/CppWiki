#ifndef CPPWIKI_TESTS_AGENT_FAKE_AGENT_TRANSPORT_H_
#define CPPWIKI_TESTS_AGENT_FAKE_AGENT_TRANSPORT_H_

#include <deque>
#include <functional>
#include <utility>
#include <vector>

#include "agent/agent_transport.h"

namespace cppwiki::agent::testing {

// A scripted AgentTransport double: returns queued outcomes in order, one per call, invoking
// the callback synchronously (no event loop needed in tests). Records every request it was
// asked to send so a test can assert on exactly what the engine sent it each round.
class FakeAgentTransport final : public AgentTransport {
 public:
  void QueueOutcome(AgentChatOutcome outcome) { queued_outcomes_.push_back(std::move(outcome)); }

  void SendChatRequest(const AgentChatRequest& request,
                       std::function<void(AgentChatOutcome)> on_done) override {
    received_requests_.push_back(request);
    if (queued_outcomes_.empty()) {
      on_done(std::string("FakeAgentTransport ran out of queued outcomes"));
      return;
    }
    auto outcome = std::move(queued_outcomes_.front());
    queued_outcomes_.pop_front();
    on_done(std::move(outcome));
  }

  [[nodiscard]] auto ReceivedRequests() const -> const std::vector<AgentChatRequest>& {
    return received_requests_;
  }

  [[nodiscard]] auto CallCount() const -> std::size_t { return received_requests_.size(); }

 private:
  std::deque<AgentChatOutcome> queued_outcomes_;
  std::vector<AgentChatRequest> received_requests_;
};

}  // namespace cppwiki::agent::testing

#endif  // CPPWIKI_TESTS_AGENT_FAKE_AGENT_TRANSPORT_H_
