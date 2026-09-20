#ifndef CPPWIKI_SRC_AGENT_AGENT_ENGINE_H_
#define CPPWIKI_SRC_AGENT_AGENT_ENGINE_H_

#include <cstddef>
#include <functional>
#include <string>
#include <variant>
#include <vector>

#include "agent/agent_message.h"
#include "agent/agent_session.h"
#include "agent/agent_tool_registry.h"
#include "agent/agent_transport.h"

namespace cppwiki::agent {

// The final assistant message the model produced with no further tool calls, or an error that
// stopped the run (a transport failure, or exceeding the tool-call round limit).
using AgentEngineOutcome = std::variant<AgentMessage, std::string>;

// The shared request -> tool call -> execute -> result -> repeat loop, per ADR-015 and the
// "Agent engine" glossary entry (doc/modules/ROOT/pages/CONTEXT.adoc). Knows nothing about what
// any tool does -- it only dispatches by name against whichever `AgentToolRegistry` it is given
// for a run. Provider-agnostic: talks to whatever `AgentTransport` it is given.
class AgentEngine final {
 public:
  // A run that keeps calling tools forever (a misbehaving or adversarial model) would hang the
  // caller indefinitely; this is the default cap on how many request/response round trips one
  // Run() performs before giving up with an error.
  static constexpr int kDefaultMaxToolCallRounds = 25;

  explicit AgentEngine(int max_tool_call_rounds = kDefaultMaxToolCallRounds)
      : max_tool_call_rounds_(max_tool_call_rounds) {}

  // Sends `session`'s current messages plus `registry`'s tool schemas to `transport`. If the
  // assistant's reply carries tool calls, each is dispatched against `registry` (in order), its
  // result appended to `session` as a tool message, and the round repeats; otherwise the
  // assistant's reply is the run's result. Every turn -- including tool results -- is appended
  // to `session` as it happens, whether the run ultimately succeeds or fails.
  //
  // `on_done` is invoked exactly once. `session`, `registry`, and `transport` must outlive the
  // run (they are captured by reference across every asynchronous round trip).
  void Run(AgentSession& session, const AgentToolRegistry& registry, AgentTransport& transport,
           std::function<void(AgentEngineOutcome)> on_done) const {
    RunRound(session, registry, transport, 0, std::move(on_done));
  }

 private:
  void RunRound(AgentSession& session, const AgentToolRegistry& registry,
               AgentTransport& transport, int round,
               std::function<void(AgentEngineOutcome)> on_done) const {
    if (round >= max_tool_call_rounds_) {
      on_done("Agent engine exceeded the maximum number of tool-call rounds (" +
              std::to_string(max_tool_call_rounds_) + ").");
      return;
    }

    const AgentChatRequest request{.messages = session.Messages(), .tools = registry.Schemas()};
    transport.SendChatRequest(
        request, [this, &session, &registry, &transport, round,
                  on_done = std::move(on_done)](AgentChatOutcome outcome) mutable {
          if (std::holds_alternative<std::string>(outcome)) {
            on_done(std::get<std::string>(std::move(outcome)));
            return;
          }

          auto assistant_message = std::get<AgentMessage>(std::move(outcome));
          const auto tool_calls = assistant_message.tool_calls;
          session.AppendMessage(std::move(assistant_message));

          if (tool_calls.empty()) {
            on_done(session.Messages().back());
            return;
          }

          DispatchToolCallsSequentially(session, registry, transport, std::move(tool_calls), 0,
                                        round, std::move(on_done));
        });
  }

  // Dispatches one assistant turn's tool calls in order, waiting for each callback before
  // starting the next. This lets a mutation tool await user confirmation without racing a
  // sibling call. The vector is moved through each callback so it survives async gaps.
  void DispatchToolCallsSequentially(
      AgentSession& session, const AgentToolRegistry& registry, AgentTransport& transport,
      std::vector<AgentToolCall> tool_calls, std::size_t index, int round,
      std::function<void(AgentEngineOutcome)> on_done) const {
    if (index >= tool_calls.size()) {
      RunRound(session, registry, transport, round + 1, std::move(on_done));
      return;
    }

    const auto call = tool_calls[index];
    registry.InvokeAsync(
        call.tool_name, call.arguments_json,
        [this, &session, &registry, &transport, tool_calls = std::move(tool_calls), index, round,
         call, on_done = std::move(on_done)](AgentToolInvocationResult result) mutable {
          AgentMessage tool_message;
          tool_message.role = AgentMessageRole::kTool;
          tool_message.tool_call_id = call.id;
          tool_message.content = result.content;
          session.AppendMessage(std::move(tool_message));
          DispatchToolCallsSequentially(session, registry, transport, std::move(tool_calls),
                                        index + 1, round, std::move(on_done));
        });
  }

  int max_tool_call_rounds_;
};

}  // namespace cppwiki::agent

#endif  // CPPWIKI_SRC_AGENT_AGENT_ENGINE_H_
