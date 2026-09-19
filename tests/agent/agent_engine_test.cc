#include "agent/agent_engine.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "agent/agent_message.h"
#include "agent/agent_session.h"
#include "agent/agent_tool_registry.h"
#include "fake_agent_transport.h"

namespace {

using cppwiki::agent::AgentEngine;
using cppwiki::agent::AgentEngineOutcome;
using cppwiki::agent::AgentMessage;
using cppwiki::agent::AgentMessageRole;
using cppwiki::agent::AgentSession;
using cppwiki::agent::AgentToolCall;
using cppwiki::agent::AgentToolInvocationResult;
using cppwiki::agent::AgentToolRegistry;
using cppwiki::agent::AgentToolSchema;
using cppwiki::agent::testing::FakeAgentTransport;

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

auto UserMessage(std::string content) -> AgentMessage {
  AgentMessage message;
  message.role = AgentMessageRole::kUser;
  message.content = std::move(content);
  return message;
}

auto FinalAssistantMessage(std::string content) -> AgentMessage {
  AgentMessage message;
  message.role = AgentMessageRole::kAssistant;
  message.content = std::move(content);
  return message;
}

auto MakeTool(std::string name) -> AgentToolSchema {
  return AgentToolSchema{.name = std::move(name), .description = "", .parameters_json_schema = ""};
}

// FakeAgentTransport invokes its callback synchronously, so Run() always completes before this
// returns -- no event loop needed in these tests.
auto RunToCompletion(const AgentEngine& engine, AgentSession& session,
                     const AgentToolRegistry& registry, FakeAgentTransport& transport)
    -> AgentEngineOutcome {
  std::optional<AgentEngineOutcome> outcome;
  engine.Run(session, registry, transport,
             [&outcome](auto result) { outcome = std::move(result); });
  Require(outcome.has_value(), "Run must call on_done exactly once for a fake transport");
  return std::move(*outcome);
}

auto TestRunReturnsTheAssistantsFinalMessageWhenItMakesNoToolCalls() -> void {
  AgentSession session("session-1");
  session.AppendMessage(UserMessage("What is CppWiki?"));

  FakeAgentTransport transport;
  transport.QueueOutcome(FinalAssistantMessage("CppWiki is an offline-first wiki."));

  const AgentToolRegistry registry;
  const AgentEngine engine;
  const auto outcome = RunToCompletion(engine, session, registry, transport);

  Require(std::holds_alternative<AgentMessage>(outcome),
          "a final assistant message with no tool calls must be reported as success");
  Require(std::get<AgentMessage>(outcome).content == "CppWiki is an offline-first wiki.",
          "the reported outcome must be the assistant's message content");
  Require(transport.CallCount() == 1, "no tool calls means exactly one round trip");
  Require(session.Messages().size() == 2,
          "the session must now hold the original user message plus the assistant's reply");
  Require(session.Messages()[1].role == AgentMessageRole::kAssistant,
          "the appended message must be the assistant's turn");
}

auto TestRunDispatchesAToolCallAndFeedsTheResultBackForAFinalReply() -> void {
  AgentSession session("session-1");
  session.AppendMessage(UserMessage("What's in the login doc?"));

  AgentMessage assistant_wants_tool;
  assistant_wants_tool.role = AgentMessageRole::kAssistant;
  assistant_wants_tool.tool_calls = {AgentToolCall{
      .id = "call-1", .tool_name = "read_document", .arguments_json = R"({"id":"login"})"}};

  FakeAgentTransport transport;
  transport.QueueOutcome(assistant_wants_tool);
  transport.QueueOutcome(FinalAssistantMessage("The login doc covers OIDC setup."));

  AgentToolRegistry registry;
  std::string received_arguments;
  registry.RegisterTool(MakeTool("read_document"),
                        [&received_arguments](const std::string& arguments_json) {
                          received_arguments = arguments_json;
                          return AgentToolInvocationResult::Ok("OIDC setup guide contents.");
                        });

  const AgentEngine engine;
  const auto outcome = RunToCompletion(engine, session, registry, transport);

  Require(std::holds_alternative<AgentMessage>(outcome),
          "a final reply after a tool call is still a success");
  Require(std::get<AgentMessage>(outcome).content == "The login doc covers OIDC setup.",
          "the reported outcome must be the second round's assistant message");
  Require(received_arguments == R"({"id":"login"})",
          "the tool handler must receive the arguments the assistant's tool call carried");
  Require(transport.CallCount() == 2,
          "a tool call round trip means two requests: before and after dispatch");

  const auto& messages = session.Messages();
  Require(messages.size() == 4,
          "the session must hold: user, assistant(tool call), tool(result), assistant(final)");
  Require(messages[1].role == AgentMessageRole::kAssistant && !messages[1].tool_calls.empty(),
          "the second message must be the assistant's tool-call turn");
  Require(messages[2].role == AgentMessageRole::kTool && messages[2].tool_call_id == "call-1",
          "the third message must be the tool result, tied back to the call that produced it");
  Require(messages[2].content == "OIDC setup guide contents.",
          "the tool message's content must be exactly what the handler returned");
  Require(messages[3].role == AgentMessageRole::kAssistant,
          "the fourth message must be the final assistant reply");

  Require(transport.ReceivedRequests()[1].messages.size() == 3,
          "the second round trip must include the user message, the tool-call turn, and the "
          "tool result");
}

auto TestRunDispatchesMultipleToolCallsFromOneAssistantTurnInOrder() -> void {
  AgentSession session("session-1");
  session.AppendMessage(UserMessage("Look up A and B"));

  AgentMessage assistant_wants_two_tools;
  assistant_wants_two_tools.role = AgentMessageRole::kAssistant;
  assistant_wants_two_tools.tool_calls = {
      AgentToolCall{.id = "call-a", .tool_name = "lookup", .arguments_json = R"({"key":"A"})"},
      AgentToolCall{.id = "call-b", .tool_name = "lookup", .arguments_json = R"({"key":"B"})"}};

  FakeAgentTransport transport;
  transport.QueueOutcome(assistant_wants_two_tools);
  transport.QueueOutcome(FinalAssistantMessage("A and B are both documented."));

  AgentToolRegistry registry;
  std::vector<std::string> invocation_order;
  registry.RegisterTool(MakeTool("lookup"), [&invocation_order](const std::string& arguments_json) {
    invocation_order.push_back(arguments_json);
    return AgentToolInvocationResult::Ok("found");
  });

  const AgentEngine engine;
  RunToCompletion(engine, session, registry, transport);

  Require(invocation_order.size() == 2,
          "both tool calls from the one assistant turn must be dispatched");
  Require(invocation_order[0] == R"({"key":"A"})" && invocation_order[1] == R"({"key":"B"})",
          "tool calls within one assistant turn must be dispatched in the order the model "
          "issued them");

  const auto& messages = session.Messages();
  Require(messages[2].tool_call_id == "call-a" && messages[3].tool_call_id == "call-b",
          "each tool call's result must be appended as its own message, in call order");
}

auto TestRunReportsATransportFailureWithoutASecondRoundTrip() -> void {
  AgentSession session("session-1");
  session.AppendMessage(UserMessage("Hello"));

  FakeAgentTransport transport;
  transport.QueueOutcome(std::string("connection refused"));

  const AgentToolRegistry registry;
  const AgentEngine engine;
  const auto outcome = RunToCompletion(engine, session, registry, transport);

  Require(std::holds_alternative<std::string>(outcome),
          "a transport failure must be reported as an error");
  Require(std::get<std::string>(outcome) == "connection refused",
          "the reported error must be the transport's own error message");
  Require(transport.CallCount() == 1, "a transport failure must not be retried");
  Require(session.Messages().size() == 1,
          "nothing should be appended to the session for a round that never produced a message");
}

auto TestRunGivesUpAfterExceedingTheMaximumToolCallRounds() -> void {
  AgentSession session("session-1");
  session.AppendMessage(UserMessage("Keep going forever"));

  FakeAgentTransport transport;
  AgentMessage always_calls_a_tool;
  always_calls_a_tool.role = AgentMessageRole::kAssistant;
  always_calls_a_tool.tool_calls = {
      AgentToolCall{.id = "call-x", .tool_name = "loop_tool", .arguments_json = "{}"}};
  constexpr int kMaxRounds = 3;
  for (int i = 0; i < kMaxRounds + 5; ++i) {
    transport.QueueOutcome(always_calls_a_tool);
  }

  AgentToolRegistry registry;
  registry.RegisterTool(MakeTool("loop_tool"), [](const std::string&) {
    return AgentToolInvocationResult::Ok("still going");
  });

  const AgentEngine engine(kMaxRounds);
  const auto outcome = RunToCompletion(engine, session, registry, transport);

  Require(std::holds_alternative<std::string>(outcome),
          "exceeding the round limit must be reported as an error, not silently truncated");
  Require(transport.CallCount() == static_cast<std::size_t>(kMaxRounds),
          "the engine must stop sending requests once the configured round limit is reached");
}

auto TestRunFeedsBackAnUnknownToolCallInsteadOfFailingTheRun() -> void {
  AgentSession session("session-1");
  session.AppendMessage(UserMessage("Do something"));

  AgentMessage assistant_hallucinates_a_tool;
  assistant_hallucinates_a_tool.role = AgentMessageRole::kAssistant;
  assistant_hallucinates_a_tool.tool_calls = {
      AgentToolCall{.id = "call-1", .tool_name = "does_not_exist", .arguments_json = "{}"}};

  FakeAgentTransport transport;
  transport.QueueOutcome(assistant_hallucinates_a_tool);
  transport.QueueOutcome(FinalAssistantMessage("Sorry, I can't do that."));

  const AgentToolRegistry registry;  // no tools registered
  const AgentEngine engine;
  const auto outcome = RunToCompletion(engine, session, registry, transport);

  Require(std::holds_alternative<AgentMessage>(outcome),
          "an unknown tool call must not fail the run -- the model gets a chance to recover");
  Require(session.Messages()[2].role == AgentMessageRole::kTool &&
              session.Messages()[2].content.find("does_not_exist") != std::string::npos,
          "the unknown-tool error must be fed back as the tool result, naming the bad tool");
}

}  // namespace

auto main() -> int {
  TestRunReturnsTheAssistantsFinalMessageWhenItMakesNoToolCalls();
  TestRunDispatchesAToolCallAndFeedsTheResultBackForAFinalReply();
  TestRunDispatchesMultipleToolCallsFromOneAssistantTurnInOrder();
  TestRunReportsATransportFailureWithoutASecondRoundTrip();
  TestRunGivesUpAfterExceedingTheMaximumToolCallRounds();
  TestRunFeedsBackAnUnknownToolCallInsteadOfFailingTheRun();
  std::cout << "cppwiki_agent_engine_tests passed\n";
  return EXIT_SUCCESS;
}
