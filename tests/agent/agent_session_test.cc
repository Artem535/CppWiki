#include "agent/agent_session.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

#include "agent/agent_message.h"

namespace {

using cppwiki::agent::AgentMessage;
using cppwiki::agent::AgentMessageRole;
using cppwiki::agent::AgentSession;

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

auto TestNewSessionHasNoMessages() -> void {
  const AgentSession session("session-1");
  Require(session.Id() == "session-1", "a session must remember the id it was constructed with");
  Require(session.Messages().empty(), "a freshly constructed session must have no messages");
}

auto TestAppendMessagePreservesOrder() -> void {
  AgentSession session("session-1");

  AgentMessage user_message;
  user_message.role = AgentMessageRole::kUser;
  user_message.content = "Fix the login bug";
  session.AppendMessage(user_message);

  AgentMessage assistant_message;
  assistant_message.role = AgentMessageRole::kAssistant;
  assistant_message.content = "Sure, looking into it.";
  session.AppendMessage(assistant_message);

  Require(session.Messages().size() == 2, "both appended messages must be present");
  Require(session.Messages()[0].content == "Fix the login bug",
          "the first appended message must stay first");
  Require(session.Messages()[1].content == "Sure, looking into it.",
          "the second appended message must stay second, after the first");
}

}  // namespace

auto main() -> int {
  TestNewSessionHasNoMessages();
  TestAppendMessagePreservesOrder();
  std::cout << "cppwiki_agent_session_tests passed\n";
  return EXIT_SUCCESS;
}
