#ifndef CPPWIKI_SRC_AGENT_AGENT_TOOL_REGISTRY_H_
#define CPPWIKI_SRC_AGENT_AGENT_TOOL_REGISTRY_H_

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace cppwiki::agent {

// One tool's OpenAI-compatible function-calling schema. `parameters_json_schema` is a raw JSON
// Schema document (opaque to the engine) describing the tool's arguments.
struct AgentToolSchema {
  std::string name;
  std::string description;
  std::string parameters_json_schema;
};

// What running one tool call produced. `is_error` distinguishes a failed invocation (unknown
// tool, handler-reported failure) from a successful one; either way `content` is fed back into
// the Agent session as the next (tool-role) message, letting the model see and react to it.
struct AgentToolInvocationResult {
  bool is_error = false;
  std::string content;

  static auto Ok(std::string content) -> AgentToolInvocationResult {
    return AgentToolInvocationResult{.is_error = false, .content = std::move(content)};
  }

  static auto Error(std::string message) -> AgentToolInvocationResult {
    return AgentToolInvocationResult{.is_error = true, .content = std::move(message)};
  }
};

using AgentToolHandler =
    std::function<AgentToolInvocationResult(const std::string& arguments_json)>;
using AgentAsyncToolHandler = std::function<void(
    const std::string& arguments_json, std::function<void(AgentToolInvocationResult)> on_done)>;

// The named set of tools (schema + handler) one mode (AI Chat, Code) configures an Agent engine
// run with, per the "Tool registry" glossary entry (doc/modules/ROOT/pages/CONTEXT.adoc). The
// registry itself has no knowledge of what any tool does -- that lives entirely in the handler
// each mode registers.
class AgentToolRegistry final {
 public:
  void RegisterTool(AgentToolSchema schema, AgentToolHandler handler) {
    handlers_.emplace(schema.name, std::move(handler));
    schemas_.push_back(std::move(schema));
  }

  void RegisterAsyncTool(AgentToolSchema schema, AgentAsyncToolHandler handler) {
    async_handlers_.emplace(schema.name, std::move(handler));
    schemas_.push_back(std::move(schema));
  }

  [[nodiscard]] auto Schemas() const -> const std::vector<AgentToolSchema>& { return schemas_; }

  [[nodiscard]] auto HasTool(const std::string& name) const -> bool {
    return handlers_.contains(name) || async_handlers_.contains(name);
  }

  // Dispatches to the handler registered under `name`. Invoking a name the registry never
  // registered is a normal, expected outcome (the model can hallucinate a tool name) -- it
  // returns an error result rather than throwing, so the engine can feed it back and let the
  // model self-correct.
  [[nodiscard]] auto Invoke(const std::string& name, const std::string& arguments_json) const
      -> AgentToolInvocationResult {
    const auto it = handlers_.find(name);
    if (it == handlers_.end()) {
      return AgentToolInvocationResult::Error("Unknown tool: " + name);
    }
    return it->second(arguments_json);
  }

  // Unified dispatch for the Agent engine. Synchronous handlers invoke on_done inline; callers
  // must not assume that this callback runs asynchronously.
  void InvokeAsync(const std::string& name, const std::string& arguments_json,
                   std::function<void(AgentToolInvocationResult)> on_done) const {
    const auto it = async_handlers_.find(name);
    if (it != async_handlers_.end()) {
      it->second(arguments_json, std::move(on_done));
      return;
    }
    on_done(Invoke(name, arguments_json));
  }

 private:
  std::vector<AgentToolSchema> schemas_;
  std::unordered_map<std::string, AgentToolHandler> handlers_;
  std::unordered_map<std::string, AgentAsyncToolHandler> async_handlers_;
};

}  // namespace cppwiki::agent

#endif  // CPPWIKI_SRC_AGENT_AGENT_TOOL_REGISTRY_H_
