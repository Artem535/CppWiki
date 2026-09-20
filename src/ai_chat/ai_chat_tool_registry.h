#ifndef CPPWIKI_SRC_AI_CHAT_AI_CHAT_TOOL_REGISTRY_H_
#define CPPWIKI_SRC_AI_CHAT_AI_CHAT_TOOL_REGISTRY_H_

#include <functional>
#include <memory>
#include <string>

#include "agent/agent_tool_confirmation_provider.h"
#include "agent/agent_tool_registry.h"
#include "bridge/editor_bridge.h"
#include "document/document_editability_gate.h"
#include "storage/local_document_repository.h"

namespace cppwiki::ai_chat {

// Registers AI Chat's document tools (read/search/mutate) against an Agent engine's tool
// registry, per ADR-020 and ADR-015: the engine itself never learns what a "document" is --
// that knowledge lives entirely here. Read/search tools touch only
// storage::LocalDocumentRepository; mutation tools additionally require a
// document::DocumentEditabilityGate pass and an agent::AgentToolConfirmationProvider
// approval (one request per tool call) before running through QEditorBridge's existing
// mutation methods.
class AiChatToolRegistry final {
 public:
  AiChatToolRegistry(agent::AgentToolRegistry& registry,
                     std::shared_ptr<storage::LocalDocumentRepository> repository,
                     bridge::QEditorBridge& editor_bridge, document::DocumentEditabilityGate& gate,
                     agent::AgentToolConfirmationProvider& confirmation_provider);

  // Registers every tool (list_documents, read_document, search_documents,
  // update_document_snapshot, rename_document, move_document, delete_document) with the
  // Agent engine's registry passed to the constructor.
  void RegisterTools();

 private:
  void RegisterListDocuments();
  void RegisterReadDocument();
  void RegisterSearchDocuments();
  void RegisterUpdateDocumentSnapshot();
  void RegisterRenameDocument();
  void RegisterMoveDocument();
  void RegisterDeleteDocument();

  // Shared by every mutation tool: gate check, then confirmation, then `apply` (which
  // performs the actual QEditorBridge call and converts its QVariantMap response).
  void RunGatedMutation(const QString& document_id, std::string tool_name, std::string summary,
                        std::function<agent::AgentToolInvocationResult()> apply,
                        std::function<void(agent::AgentToolInvocationResult)> on_done);

  agent::AgentToolRegistry& registry_;
  std::shared_ptr<storage::LocalDocumentRepository> repository_;
  bridge::QEditorBridge& editor_bridge_;
  document::DocumentEditabilityGate& gate_;
  agent::AgentToolConfirmationProvider& confirmation_provider_;
};

}  // namespace cppwiki::ai_chat

#endif  // CPPWIKI_SRC_AI_CHAT_AI_CHAT_TOOL_REGISTRY_H_
