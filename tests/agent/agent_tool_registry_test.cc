#include "agent/agent_tool_registry.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

using cppwiki::agent::AgentToolInvocationResult;
using cppwiki::agent::AgentToolRegistry;
using cppwiki::agent::AgentToolSchema;

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

auto TestRegisteredToolSchemasArePreservedInRegistrationOrder() -> void {
  AgentToolRegistry registry;
  registry.RegisterTool(
      AgentToolSchema{.name = "search_documents", .description = "Search the wiki",
                      .parameters_json_schema = R"({"type":"object"})"},
      [](const std::string&) { return AgentToolInvocationResult::Ok("[]"); });
  registry.RegisterTool(
      AgentToolSchema{.name = "read_document", .description = "Read a wiki document",
                      .parameters_json_schema = R"({"type":"object"})"},
      [](const std::string&) { return AgentToolInvocationResult::Ok("{}"); });

  const auto& schemas = registry.Schemas();
  Require(schemas.size() == 2, "both registered tool schemas must be reported");
  Require(schemas[0].name == "search_documents", "schemas must be reported in registration order");
  Require(schemas[1].name == "read_document", "schemas must be reported in registration order");
}

auto TestInvokeDispatchesToTheMatchingHandlerWithItsArguments() -> void {
  AgentToolRegistry registry;
  std::string received_arguments;
  registry.RegisterTool(
      AgentToolSchema{.name = "search_documents", .description = "Search the wiki",
                      .parameters_json_schema = R"({"type":"object"})"},
      [&received_arguments](const std::string& arguments_json) {
        received_arguments = arguments_json;
        return AgentToolInvocationResult::Ok(R"([{"id":"doc-1"}])");
      });

  const auto result = registry.Invoke("search_documents", R"({"query":"login bug"})");

  Require(received_arguments == R"({"query":"login bug"})",
          "the handler must receive the exact arguments passed to Invoke");
  Require(!result.is_error, "invoking a registered tool must not be reported as an error");
  Require(result.content == R"([{"id":"doc-1"}])",
          "Invoke must return exactly what the handler produced");
}

auto TestInvokeOfAnUnknownToolReturnsAnErrorInsteadOfCrashing() -> void {
  AgentToolRegistry registry;

  const auto result = registry.Invoke("does_not_exist", "{}");

  Require(result.is_error, "invoking a tool the registry never registered must be an error");
  Require(result.content.find("does_not_exist") != std::string::npos,
          "the error message should name the unknown tool for diagnosability");
}

auto TestHasToolReflectsWhatWasRegistered() -> void {
  AgentToolRegistry registry;
  Require(!registry.HasTool("search_documents"), "an unregistered tool must not be reported");

  registry.RegisterTool(
      AgentToolSchema{.name = "search_documents", .description = "Search the wiki",
                      .parameters_json_schema = R"({"type":"object"})"},
      [](const std::string&) { return AgentToolInvocationResult::Ok("[]"); });

  Require(registry.HasTool("search_documents"), "a registered tool must be reported as present");
}

}  // namespace

auto main() -> int {
  TestRegisteredToolSchemasArePreservedInRegistrationOrder();
  TestInvokeDispatchesToTheMatchingHandlerWithItsArguments();
  TestInvokeOfAnUnknownToolReturnsAnErrorInsteadOfCrashing();
  TestHasToolReflectsWhatWasRegistered();
  std::cout << "cppwiki_agent_tool_registry_tests passed\n";
  return EXIT_SUCCESS;
}
