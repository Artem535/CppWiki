#include "ai_chat/ai_chat_tool_registry.h"

#include <QString>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "agent/agent_tool_registry.h"
#include "bridge/editor_bridge.h"
#include "document/document_editability_gate.h"
#include "fake_agent_tool_confirmation_provider.h"
#include "storage/fake_document_repository.h"

namespace {

using cppwiki::agent::AgentToolInvocationResult;
using cppwiki::agent::AgentToolRegistry;
using cppwiki::agent::testing::FakeAgentToolConfirmationProvider;
using cppwiki::ai_chat::AiChatToolRegistry;
using cppwiki::bridge::QEditorBridge;
using cppwiki::document::DocumentEditabilityGate;
using cppwiki::document::LockStatusCheckResult;
using cppwiki::storage::DocumentConflictRecord;
using cppwiki::storage::DocumentRecord;
using cppwiki::storage::testing::FakeDocumentRepository;

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

auto AlwaysUnlockedGate(std::shared_ptr<FakeDocumentRepository> repository)
    -> DocumentEditabilityGate {
  return DocumentEditabilityGate(
      repository, [](const QString&, std::function<void(LockStatusCheckResult)> on_done) {
        on_done(LockStatusCheckResult{.checked_successfully = true});
      });
}

auto SeedWikiPage(FakeDocumentRepository& repository, const std::string& id,
                  const std::string& title, const std::string& body_text) -> void {
  DocumentRecord record;
  record.metadata.id = id;
  record.metadata.title = title;
  record.metadata.workspace_id = "default";
  record.snapshot.id = id;
  record.snapshot.schema_version = 1;
  record.snapshot.title = title;
  record.snapshot.blocks = std::vector<cppwiki::document::BlockNoteBlockSnapshot>{
      cppwiki::document::BlockNoteBlockSnapshot{
          .id = std::string("b1"),
          .type = std::string("paragraph"),
          .content = std::make_optional(rfl::Generic(body_text)),
      }};
  record.raw_snapshot_json =
      R"({"blocks":[{"id":"b1","type":"paragraph","content":[{"type":"text","text":")" + body_text +
      R"("}]}]})";
  static_cast<void>(repository.SaveDocument(record));
}

struct Fixture {
  std::shared_ptr<FakeDocumentRepository> repository = std::make_shared<FakeDocumentRepository>();
  QEditorBridge editor_bridge;
  DocumentEditabilityGate gate = AlwaysUnlockedGate(repository);
  FakeAgentToolConfirmationProvider confirmation_provider;
  AgentToolRegistry engine_registry;

  Fixture() {
    editor_bridge.SetRepository(repository);
  }
};

auto InvokeAndCapture(const AgentToolRegistry& registry, const std::string& name,
                      const std::string& arguments_json) -> AgentToolInvocationResult {
  std::optional<AgentToolInvocationResult> result;
  registry.InvokeAsync(name, arguments_json, [&result](auto r) { result = std::move(r); });
  Require(result.has_value(),
          "every registered AI Chat tool must call on_done synchronously for a fake "
          "gate/confirmation provider");
  return *result;
}

auto TestListDocumentsReturnsEverySeededDocument() -> void {
  Fixture fixture;
  SeedWikiPage(*fixture.repository, "doc-1", "Login flow", "Explains OIDC.");
  SeedWikiPage(*fixture.repository, "doc-2", "Sync model", "Explains conflicts.");
  AiChatToolRegistry ai_chat_tools(fixture.engine_registry, fixture.repository,
                                   fixture.editor_bridge, fixture.gate,
                                   fixture.confirmation_provider);
  ai_chat_tools.RegisterTools();

  const auto result = InvokeAndCapture(fixture.engine_registry, "list_documents", "{}");

  Require(!result.is_error, "listing documents must not be an error");
  Require(result.content.find("doc-1") != std::string::npos &&
              result.content.find("doc-2") != std::string::npos,
          "list_documents must mention every seeded document id");
}

auto TestReadDocumentReturnsTitleAndPlainText() -> void {
  Fixture fixture;
  SeedWikiPage(*fixture.repository, "doc-1", "Login flow", "Explains OIDC.");
  AiChatToolRegistry ai_chat_tools(fixture.engine_registry, fixture.repository,
                                   fixture.editor_bridge, fixture.gate,
                                   fixture.confirmation_provider);
  ai_chat_tools.RegisterTools();

  const auto result =
      InvokeAndCapture(fixture.engine_registry, "read_document", R"({"document_id":"doc-1"})");

  Require(!result.is_error, "reading a seeded document must not be an error");
  Require(result.content.find("Login flow") != std::string::npos &&
              result.content.find("Explains OIDC.") != std::string::npos,
          "read_document must return the title and plain-text body");
}

auto TestSearchDocumentsMatchesTitleAndBodySubstring() -> void {
  Fixture fixture;
  SeedWikiPage(*fixture.repository, "doc-1", "Login flow", "Explains OIDC setup end to end.");
  SeedWikiPage(*fixture.repository, "doc-2", "Sync model", "Explains conflict resolution.");
  AiChatToolRegistry ai_chat_tools(fixture.engine_registry, fixture.repository,
                                   fixture.editor_bridge, fixture.gate,
                                   fixture.confirmation_provider);
  ai_chat_tools.RegisterTools();

  const auto by_title =
      InvokeAndCapture(fixture.engine_registry, "search_documents", R"({"query":"login"})");
  Require(by_title.content.find("doc-1") != std::string::npos &&
              by_title.content.find("doc-2") == std::string::npos,
          "a query matching the title must find doc-1 only");

  const auto by_body =
      InvokeAndCapture(fixture.engine_registry, "search_documents", R"({"query":"OIDC"})");
  Require(by_body.content.find("doc-1") != std::string::npos,
          "a query matching body text must find doc-1 too");
}

auto TestUpdateDocumentSnapshotBlockedWhenConflicted() -> void {
  Fixture fixture;
  SeedWikiPage(*fixture.repository, "doc-1", "Login flow", "original");
  static_cast<void>(fixture.repository->SaveConflict(DocumentConflictRecord{
      .id = "conflict-1", .document_id = "doc-1", .resolution_state = "pending"}));
  AiChatToolRegistry ai_chat_tools(fixture.engine_registry, fixture.repository,
                                   fixture.editor_bridge, fixture.gate,
                                   fixture.confirmation_provider);
  ai_chat_tools.RegisterTools();

  const auto result = InvokeAndCapture(
      fixture.engine_registry, "update_document_snapshot",
      R"({"document_id":"doc-1","snapshot_json":"{\"id\":\"doc-1\",\"schema_version\":1,\"blocks\":[]}"})");

  Require(result.is_error, "a mutation on a conflicted document must be rejected");
  Require(fixture.confirmation_provider.ReceivedRequests().empty(),
          "the gate must reject before ever asking for confirmation");
}

auto TestUpdateDocumentSnapshotBlockedWhenConfirmationRejected() -> void {
  Fixture fixture;
  SeedWikiPage(*fixture.repository, "doc-1", "Login flow", "original");
  fixture.confirmation_provider.QueueDecision(false);
  AiChatToolRegistry ai_chat_tools(fixture.engine_registry, fixture.repository,
                                   fixture.editor_bridge, fixture.gate,
                                   fixture.confirmation_provider);
  ai_chat_tools.RegisterTools();

  const auto result =
      InvokeAndCapture(fixture.engine_registry, "update_document_snapshot",
                       R"({"document_id":"doc-1","snapshot_json":"{\"blocks\":[]}"})");

  Require(result.is_error, "a rejected confirmation must not apply the mutation");
  Require(fixture.confirmation_provider.ReceivedRequests().size() == 1,
          "exactly one confirmation request must have been made");

  const auto loaded = fixture.repository->LoadDocument("doc-1");
  Require(loaded.document.has_value() &&
              loaded.document->raw_snapshot_json.find("\"blocks\":[]") == std::string::npos,
          "the document must be unchanged after a rejected confirmation");
}

auto TestUpdateDocumentSnapshotAppliesWhenGateAndConfirmationBothPass() -> void {
  Fixture fixture;
  SeedWikiPage(*fixture.repository, "doc-1", "Login flow", "original");
  fixture.confirmation_provider.QueueDecision(true);
  AiChatToolRegistry ai_chat_tools(fixture.engine_registry, fixture.repository,
                                   fixture.editor_bridge, fixture.gate,
                                   fixture.confirmation_provider);
  ai_chat_tools.RegisterTools();

  const auto result = InvokeAndCapture(
      fixture.engine_registry, "update_document_snapshot",
      R"({"document_id":"doc-1","snapshot_json":"{\"id\":\"doc-1\",\"schema_version\":1,\"blocks\":[{\"id\":\"b1\",\"type\":\"paragraph\",\"content\":[{\"type\":\"text\",\"text\":\"updated\"}]}]}"})");

  Require(!result.is_error, "an approved mutation on an editable document must succeed");
  const auto loaded = fixture.repository->LoadDocument("doc-1");
  Require(loaded.document.has_value() &&
              loaded.document->raw_snapshot_json.find("updated") != std::string::npos,
          "the document's content must reflect the applied mutation");
}

auto TestRenameDocumentAppliesWhenApproved() -> void {
  Fixture fixture;
  SeedWikiPage(*fixture.repository, "doc-1", "Old title", "body");
  fixture.confirmation_provider.QueueDecision(true);
  AiChatToolRegistry ai_chat_tools(fixture.engine_registry, fixture.repository,
                                   fixture.editor_bridge, fixture.gate,
                                   fixture.confirmation_provider);
  ai_chat_tools.RegisterTools();

  const auto result = InvokeAndCapture(fixture.engine_registry, "rename_document",
                                       R"({"document_id":"doc-1","title":"New title"})");

  Require(!result.is_error, "an approved rename must succeed");
  const auto loaded = fixture.repository->LoadDocument("doc-1");
  Require(loaded.document.has_value() && loaded.document->metadata.title == "New title",
          "the document's title must be updated");
}

auto TestDeleteDocumentAppliesWhenApproved() -> void {
  Fixture fixture;
  SeedWikiPage(*fixture.repository, "doc-1", "Doomed", "body");
  fixture.confirmation_provider.QueueDecision(true);
  AiChatToolRegistry ai_chat_tools(fixture.engine_registry, fixture.repository,
                                   fixture.editor_bridge, fixture.gate,
                                   fixture.confirmation_provider);
  ai_chat_tools.RegisterTools();

  const auto result =
      InvokeAndCapture(fixture.engine_registry, "delete_document", R"({"document_id":"doc-1"})");

  Require(!result.is_error, "an approved delete must succeed");
}

}  // namespace

auto main() -> int {
  TestListDocumentsReturnsEverySeededDocument();
  TestReadDocumentReturnsTitleAndPlainText();
  TestSearchDocumentsMatchesTitleAndBodySubstring();
  TestUpdateDocumentSnapshotBlockedWhenConflicted();
  TestUpdateDocumentSnapshotBlockedWhenConfirmationRejected();
  TestUpdateDocumentSnapshotAppliesWhenGateAndConfirmationBothPass();
  TestRenameDocumentAppliesWhenApproved();
  TestDeleteDocumentAppliesWhenApproved();
  std::cout << "cppwiki_ai_chat_tool_registry_tests passed\n";
  return EXIT_SUCCESS;
}
