#ifndef CPPWIKI_SRC_AGENT_AGENT_SESSION_H_
#define CPPWIKI_SRC_AGENT_AGENT_SESSION_H_

#include <string>
#include <utility>
#include <vector>

#include "agent/agent_message.h"

namespace cppwiki::agent {

// The persisted sequence of agent messages belonging to one Agent engine run, per the
// "Agent session" glossary entry (doc/modules/ROOT/pages/CONTEXT.adoc). Data model only --
// no list-view-of-sessions UI here (that's mode-specific, ADR-014).
class AgentSession final {
 public:
  explicit AgentSession(std::string id) : id_(std::move(id)) {}

  [[nodiscard]] auto Id() const -> const std::string& { return id_; }
  [[nodiscard]] auto Messages() const -> const std::vector<AgentMessage>& { return messages_; }

  void AppendMessage(AgentMessage message) { messages_.push_back(std::move(message)); }

 private:
  std::string id_;
  std::vector<AgentMessage> messages_;
};

}  // namespace cppwiki::agent

#endif  // CPPWIKI_SRC_AGENT_AGENT_SESSION_H_
