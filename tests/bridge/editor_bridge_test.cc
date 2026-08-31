#include "bridge/editor_bridge.h"

#include <spdlog/spdlog.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>
#include <algorithm>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "core/constants.h"
#include "core/qt_string.h"
#include "storage/local_document_repository.h"

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto RequireSuccessEnvelope(const QVariantMap& response) -> void {
  Require(
      response.value(QStringLiteral("apiVersion")).toInt() == cppwiki::constants::kBridgeApiVersion,
      "response apiVersion must be 1");
  Require(response.value(QStringLiteral("ok")).toBool(), "response must be ok");
  Require(response.contains(QStringLiteral("result")), "result must exist");
}

auto RequireErrorEnvelope(const QVariantMap& response, const QString& expected_code) -> void {
  Require(
      response.value(QStringLiteral("apiVersion")).toInt() == cppwiki::constants::kBridgeApiVersion,
      "error response apiVersion must be 1");
  Require(!response.value(QStringLiteral("ok")).toBool(), "error response must not be ok");

  const auto error = response.value(QStringLiteral("error")).toMap();
  Require(error.value(QStringLiteral("code")).toString() == expected_code, "error code mismatch");
  Require(!error.value(QStringLiteral("message")).toString().isEmpty(),
          "error message must not be empty");
}

class FakeSyncStateProvider final : public cppwiki::sync::SyncStateProvider {
 public:
  explicit FakeSyncStateProvider(QStringList remote_workspaces = {})
      : remote_workspaces_(std::move(remote_workspaces)) {}

  [[nodiscard]] auto ShouldExpectRemoteDocuments(const QString& workspace_id) const
      -> bool override {
    return remote_workspaces_.contains(workspace_id.trimmed().isEmpty() ? QStringLiteral("default")
                                                                        : workspace_id.trimmed());
  }

  [[nodiscard]] auto ShouldCreateSyntheticWelcomePage(const QString& workspace_id) const
      -> bool override {
    return !ShouldExpectRemoteDocuments(workspace_id);
  }

 private:
  QStringList remote_workspaces_;
};

class FakeDocumentRepository final : public cppwiki::storage::LocalDocumentRepository {
 public:
  [[nodiscard]] auto SaveDocument(const cppwiki::storage::DocumentRecord& document)
      -> cppwiki::storage::SaveDocumentResult override {
    documents_[document.metadata.id] = document;
    return cppwiki::storage::SaveDocumentResult{};
  }

  [[nodiscard]] auto DeleteDocument(std::string_view page_id)
      -> cppwiki::storage::DeleteDocumentResult override {
    documents_.erase(std::string(page_id));
    return cppwiki::storage::DeleteDocumentResult{};
  }

  [[nodiscard]] auto LoadDocument(std::string_view page_id)
      -> cppwiki::storage::LoadDocumentResult override {
    const auto it = documents_.find(std::string(page_id));
    if (it == documents_.end()) {
      return cppwiki::storage::LoadDocumentResult{
          .document = std::nullopt,
          .error =
              cppwiki::storage::RepositoryError{
                  .code = cppwiki::storage::RepositoryErrorCode::kReadFailed,
                  .message = "Document was not found.",
              },
      };
    }

    return cppwiki::storage::LoadDocumentResult{
        .document = it->second,
        .error = std::nullopt,
    };
  }

  [[nodiscard]] auto ListDocuments() -> cppwiki::storage::ListDocumentsResult override {
    cppwiki::storage::ListDocumentsResult result;
    for (const auto& [id, document] : documents_) {
      result.documents.push_back(cppwiki::storage::DocumentSummaryFromMetadata(document.metadata));
    }
    return result;
  }

  [[nodiscard]] auto SaveAttachment(const cppwiki::storage::AttachmentData& attachment)
      -> cppwiki::storage::SaveAttachmentResult override {
    attachments_[attachment.metadata.id] = attachment;
    return {};
  }

  [[nodiscard]] auto LoadAttachment(std::string_view attachment_id, std::string_view workspace_id)
      -> cppwiki::storage::LoadAttachmentResult override {
    const auto it = attachments_.find(std::string(attachment_id));
    if (it == attachments_.end() || it->second.metadata.workspace_id != workspace_id) {
      return {.attachment = std::nullopt,
              .error = cppwiki::storage::RepositoryError{
                  .code = cppwiki::storage::RepositoryErrorCode::kReadFailed,
                  .message = "Attachment was not found.",
              }};
    }
    return {.attachment = it->second, .error = std::nullopt};
  }

  [[nodiscard]] auto ListAttachments(std::string_view workspace_id)
      -> cppwiki::storage::ListAttachmentsResult override {
    cppwiki::storage::ListAttachmentsResult result;
    for (const auto& [id, attachment] : attachments_) {
      if (attachment.metadata.workspace_id == workspace_id) {
        result.attachments.push_back(attachment.metadata);
      }
    }
    return result;
  }

  [[nodiscard]] auto SaveConflict(const cppwiki::storage::DocumentConflictRecord&)
      -> cppwiki::storage::SaveConflictResult override {
    return {};
  }

  [[nodiscard]] auto DeleteConflict(std::string_view)
      -> cppwiki::storage::DeleteConflictResult override {
    return {};
  }

  [[nodiscard]] auto LoadConflict(std::string_view)
      -> cppwiki::storage::LoadConflictResult override {
    return {};
  }

  [[nodiscard]] auto ListConflicts() -> cppwiki::storage::ListConflictsResult override {
    return {};
  }

  [[nodiscard]] auto ResolveConflict(std::string_view)
      -> cppwiki::storage::UpdateConflictResolutionResult override {
    return {};
  }

  [[nodiscard]] auto DismissConflict(std::string_view)
      -> cppwiki::storage::UpdateConflictResolutionResult override {
    return {};
  }

  [[nodiscard]] auto SaveWorkspaceRoot(const cppwiki::storage::WorkspaceRootRecord& root)
      -> cppwiki::storage::SaveWorkspaceRootResult override {
    workspace_roots_[root.workspace_id] = root;
    return {};
  }

  [[nodiscard]] auto LoadWorkspaceRoot(std::string_view workspace_id)
      -> std::optional<cppwiki::storage::WorkspaceRootRecord> override {
    const auto it = workspace_roots_.find(std::string(workspace_id));
    if (it == workspace_roots_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  [[nodiscard]] auto SupportsSync() const -> bool override {
    return true;
  }

  [[nodiscard]] auto SaveDocumentRevision(const cppwiki::storage::DocumentRevisionRecord& revision)
      -> cppwiki::storage::SaveDocumentRevisionResult override {
    revisions_[revision.id] = revision;
    return {};
  }

  [[nodiscard]] auto ListDocumentRevisions(std::string_view document_id)
      -> cppwiki::storage::ListDocumentRevisionsResult override {
    cppwiki::storage::ListDocumentRevisionsResult result;
    for (const auto& [id, revision] : revisions_) {
      if (revision.document_id == document_id) {
        result.revisions.push_back(revision);
      }
    }
    std::ranges::sort(result.revisions,
                      [](const auto& lhs, const auto& rhs) { return lhs.saved_at > rhs.saved_at; });
    return result;
  }

  [[nodiscard]] auto DeleteDocumentRevision(std::string_view revision_id)
      -> cppwiki::storage::DeleteDocumentRevisionResult override {
    revisions_.erase(std::string(revision_id));
    return {};
  }

 private:
  std::map<std::string, cppwiki::storage::DocumentRecord> documents_;
  std::map<std::string, cppwiki::storage::AttachmentData> attachments_;
  std::map<std::string, cppwiki::storage::DocumentRevisionRecord> revisions_;
  std::map<std::string, cppwiki::storage::WorkspaceRootRecord> workspace_roots_;
};

auto TestBridgeInfo() -> void {
  cppwiki::bridge::QEditorBridge bridge;
  const auto response = bridge.getBridgeInfo();

  RequireSuccessEnvelope(response);

  const auto result = response.value(QStringLiteral("result")).toMap();
  Require(
      result.value(QStringLiteral("apiVersion")).toInt() == cppwiki::constants::kBridgeApiVersion,
      "bridge info apiVersion must be 1");
  Require(result.value(QStringLiteral("namespace")).toString() ==
              cppwiki::ToQString(cppwiki::constants::kDocumentsBridgeNamespace),
          "bridge namespace mismatch");

  const auto methods = result.value(QStringLiteral("methods")).toList();
  Require(methods.contains(cppwiki::ToQString(cppwiki::constants::kBridgeMethodGetBridgeInfo)),
          "missing info method");
  Require(methods.contains(cppwiki::ToQString(cppwiki::constants::kBridgeMethodGetInitialDocument)),
          "missing initial document method");
  Require(methods.contains(cppwiki::ToQString(cppwiki::constants::kBridgeMethodListDocuments)),
          "missing list documents method");
  Require(methods.contains(cppwiki::ToQString(cppwiki::constants::kBridgeMethodCreateDocument)),
          "missing create document method");
  Require(
      methods.contains(cppwiki::ToQString(cppwiki::constants::kBridgeMethodCreateChildDocument)),
      "missing create child document method");
  Require(methods.contains(cppwiki::ToQString(cppwiki::constants::kBridgeMethodRenameDocument)),
          "missing rename document method");
  Require(methods.contains(cppwiki::ToQString(cppwiki::constants::kBridgeMethodLoadDocument)),
          "missing load document method");
  Require(methods.contains(cppwiki::ToQString(cppwiki::constants::kBridgeMethodOpenDocument)),
          "missing open document method");
  Require(methods.contains(cppwiki::ToQString(cppwiki::constants::kBridgeMethodUpdateSnapshot)),
          "missing update snapshot method");
}

auto TestInitialDocumentStartsEmpty() -> void {
  cppwiki::bridge::QEditorBridge bridge;
  const auto response = bridge.getInitialDocument();

  RequireSuccessEnvelope(response);

  const auto blocks = response.value(QStringLiteral("result")).toList();
  Require(blocks.empty(), "initial document should be empty until a page is selected");
}

auto TestDocumentListBootstrapsWelcomePage() -> QString {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);
  bridge.SetSyncStateProvider(nullptr);

  const auto response = bridge.listDocuments();
  RequireSuccessEnvelope(response);

  const auto pages = response.value(QStringLiteral("result")).toList();
  Require(pages.size() == 1, "empty repository should be bootstrapped with one page");

  const auto page = pages.front().toMap();
  Require(page.value(QStringLiteral("title")).toString() == QStringLiteral("Welcome to CppWiki"),
          "bootstrap page title should match");
  Require(page.value(QStringLiteral("workspaceId")).toString() == QStringLiteral("default"),
          "bootstrap page workspace should default to default");
  Require(!page.value(QStringLiteral("createdBy")).toString().isEmpty(),
          "bootstrap page creator should not be empty");
  Require(page.value(QStringLiteral("updatedBy")).toString() ==
              page.value(QStringLiteral("createdBy")).toString(),
          "bootstrap page updatedBy should initially match createdBy");
  Require(page.value(QStringLiteral("contentVersion")).toLongLong() == 1,
          "bootstrap page contentVersion should start at 1");
  return page.value(QStringLiteral("id")).toString();
}

auto TestCreateDocument() -> QString {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto response = bridge.createDocument();
  RequireSuccessEnvelope(response);

  const auto created = response.value(QStringLiteral("result")).toMap();
  Require(created.value(QStringLiteral("title")).toString() == QStringLiteral("Untitled note"),
          "created document title should match");
  Require(!created.value(QStringLiteral("id")).toString().isEmpty(),
          "created document id should not be empty");
  Require(created.value(QStringLiteral("parentId")).isNull() ||
              !created.value(QStringLiteral("parentId")).isValid(),
          "created root document should not have a parent");
  Require(created.value(QStringLiteral("workspaceId")).toString() == QStringLiteral("default"),
          "created document workspace should default to default");
  Require(!created.value(QStringLiteral("createdBy")).toString().isEmpty(),
          "created document creator should not be empty");
  Require(created.value(QStringLiteral("updatedBy")).toString() ==
              created.value(QStringLiteral("createdBy")).toString(),
          "created document updatedBy should initially match createdBy");
  Require(created.value(QStringLiteral("contentVersion")).toLongLong() == 1,
          "created document contentVersion should start at 1");
  return created.value(QStringLiteral("id")).toString();
}

auto TestCreateDocumentLoadsEmptyAndSaves() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto created = bridge.createDocument();
  RequireSuccessEnvelope(created);
  const auto created_id =
      created.value(QStringLiteral("result")).toMap().value(QStringLiteral("id")).toString();

  const auto loaded = bridge.loadDocument(created_id);
  RequireSuccessEnvelope(loaded);
  Require(loaded.value(QStringLiteral("result"))
              .toMap()
              .value(QStringLiteral("blocks"))
              .toList()
              .isEmpty(),
          "new document should load with no initial blocks");
  Require(loaded.value(QStringLiteral("result")).toMap().value(QStringLiteral("kind")).toString() ==
              QStringLiteral("wikiPage"),
          "newly created document should default to kind 'wikiPage' in the bridge payload");

  const auto saved = bridge.updateSnapshot(created_id, QStringLiteral(R"([
    {
      "id": "b1",
      "type": "paragraph",
      "content": [
        { "type": "text", "text": "Body only", "styles": {} }
      ],
      "children": []
    }
  ])"));
  RequireSuccessEnvelope(saved);

  const auto reloaded = bridge.loadDocument(created_id);
  RequireSuccessEnvelope(reloaded);
  Require(
      reloaded.value(QStringLiteral("result")).toMap().value(QStringLiteral("title")).toString() ==
          QStringLiteral("Untitled note"),
      "document without h1 should keep its existing title");
  Require(reloaded.value(QStringLiteral("result"))
                  .toMap()
                  .value(QStringLiteral("contentVersion"))
                  .toLongLong() == 2,
          "saving a snapshot should increment contentVersion");
}

auto TestCreateDocumentDoesNotHijackAutosaveSelection() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto listed = bridge.listDocuments();
  RequireSuccessEnvelope(listed);
  const auto welcome_id = listed.value(QStringLiteral("result"))
                              .toList()
                              .front()
                              .toMap()
                              .value(QStringLiteral("id"))
                              .toString();

  RequireSuccessEnvelope(bridge.openDocument(welcome_id));

  const auto created = bridge.createDocument();
  RequireSuccessEnvelope(created);
  const auto created_id =
      created.value(QStringLiteral("result")).toMap().value(QStringLiteral("id")).toString();

  const auto saved = bridge.updateSnapshot(welcome_id, QStringLiteral(R"([
    {
      "id": "welcome-heading",
      "type": "heading",
      "props": { "level": 1 },
      "content": [
        { "type": "text", "text": "Welcome heading", "styles": {} }
      ],
      "children": []
    }
  ])"));
  RequireSuccessEnvelope(saved);

  const auto loaded_created = bridge.loadDocument(created_id);
  RequireSuccessEnvelope(loaded_created);
  const auto created_result = loaded_created.value(QStringLiteral("result")).toMap();
  Require(
      created_result.value(QStringLiteral("title")).toString() == QStringLiteral("Untitled note"),
      "creating a document should not change bridge selection before open");
  Require(
      created_result.value(QStringLiteral("workspaceId")).toString() == QStringLiteral("default"),
      "loaded created document workspace should default to default");
  Require(!created_result.value(QStringLiteral("createdBy")).toString().isEmpty(),
          "loaded created document creator should not be empty");
  Require(created_result.value(QStringLiteral("blocks")).toList().isEmpty(),
          "newly created document should remain empty until it is opened and edited");

  const auto loaded_welcome = bridge.loadDocument(welcome_id);
  RequireSuccessEnvelope(loaded_welcome);
  const auto welcome_result = loaded_welcome.value(QStringLiteral("result")).toMap();
  Require(
      welcome_result.value(QStringLiteral("title")).toString() == QStringLiteral("Welcome heading"),
      "autosave should still apply to the previously opened document");
}

auto TestRenameDocumentUpdatesTitle() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto listed = bridge.listDocuments();
  RequireSuccessEnvelope(listed);
  const auto page_id = listed.value(QStringLiteral("result"))
                           .toList()
                           .front()
                           .toMap()
                           .value(QStringLiteral("id"))
                           .toString();

  const auto renamed = bridge.renameDocument(page_id, QStringLiteral("Renamed title"));
  RequireSuccessEnvelope(renamed);
  Require(
      renamed.value(QStringLiteral("result")).toMap().value(QStringLiteral("title")).toString() ==
          QStringLiteral("Renamed title"),
      "rename should return the updated title");

  const auto reloaded = bridge.loadDocument(page_id);
  RequireSuccessEnvelope(reloaded);
  Require(
      reloaded.value(QStringLiteral("result")).toMap().value(QStringLiteral("title")).toString() ==
          QStringLiteral("Renamed title"),
      "rename should persist in loadDocument");

  const auto relisted = bridge.listDocuments();
  RequireSuccessEnvelope(relisted);
  Require(relisted.value(QStringLiteral("result"))
                  .toList()
                  .front()
                  .toMap()
                  .value(QStringLiteral("title"))
                  .toString() == QStringLiteral("Renamed title"),
          "rename should persist in listDocuments");
}

auto TestDeleteDocumentRemovesItFromList() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto listed_before = bridge.listDocuments();
  RequireSuccessEnvelope(listed_before);
  const auto page_id = listed_before.value(QStringLiteral("result"))
                           .toList()
                           .front()
                           .toMap()
                           .value(QStringLiteral("id"))
                           .toString();

  const auto deleted = bridge.deleteDocument(page_id);
  RequireSuccessEnvelope(deleted);

  const auto listed = bridge.listDocuments();
  RequireSuccessEnvelope(listed);
  const auto pages = listed.value(QStringLiteral("result")).toList();
  Require(pages.isEmpty(), "deleting the final page must leave the normal document list empty");

  const auto trash = bridge.listTrash();
  RequireSuccessEnvelope(trash);
  Require(trash.value(QStringLiteral("result")).toList().size() == 1,
          "deleting the final page must retain it in the trash");
}

// Workspaces created before workspace-root records were introduced still contain the trashed
// document itself. That is sufficient evidence that the workspace is not new and must not
// bootstrap a replacement Welcome page after its final document is deleted.
auto TestDeleteDocumentFromPreRootWorkspaceLeavesNormalListEmpty() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto created = bridge.createDocument();
  RequireSuccessEnvelope(created);
  const auto page_id =
      created.value(QStringLiteral("result")).toMap().value(QStringLiteral("id")).toString();

  RequireSuccessEnvelope(bridge.deleteDocument(page_id));

  const auto listed = bridge.listDocuments();
  RequireSuccessEnvelope(listed);
  Require(listed.value(QStringLiteral("result")).toList().isEmpty(),
          "a trashed legacy document must prevent Welcome bootstrap");
}

// Issue #165: "deleteDocument" now soft-deletes -- the page must disappear from listDocuments()
// (covered above) but reappear in listTrash(), still fully intact, until it's restored or
// permanently deleted.
auto TestDeleteDocumentMovesItToTrashInsteadOfErasingIt() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto listed_before = bridge.listDocuments();
  RequireSuccessEnvelope(listed_before);
  const auto page_id = listed_before.value(QStringLiteral("result"))
                           .toList()
                           .front()
                           .toMap()
                           .value(QStringLiteral("id"))
                           .toString();

  RequireSuccessEnvelope(bridge.deleteDocument(page_id));

  const auto trash = bridge.listTrash();
  RequireSuccessEnvelope(trash);
  const auto trashed_pages = trash.value(QStringLiteral("result")).toList();
  Require(trashed_pages.size() == 1, "the deleted page should appear in listTrash");
  const auto trashed_page = trashed_pages.front().toMap();
  Require(trashed_page.value(QStringLiteral("id")).toString() == page_id,
          "listTrash should report the deleted page's own id");
  Require(!trashed_page.value(QStringLiteral("trashedAt")).toString().isEmpty(),
          "listTrash should report a non-empty trashedAt timestamp");
}

auto TestRestoreDocumentBringsItBackToTheNormalList() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto listed_before = bridge.listDocuments();
  RequireSuccessEnvelope(listed_before);
  const auto page_id = listed_before.value(QStringLiteral("result"))
                           .toList()
                           .front()
                           .toMap()
                           .value(QStringLiteral("id"))
                           .toString();

  RequireSuccessEnvelope(bridge.deleteDocument(page_id));

  const auto listed_while_trashed = bridge.listDocuments();
  RequireSuccessEnvelope(listed_while_trashed);
  Require(listed_while_trashed.value(QStringLiteral("result")).toList().isEmpty(),
          "the normal list must remain empty until the page is restored");

  RequireSuccessEnvelope(bridge.restoreDocument(page_id));

  const auto listed_after = bridge.listDocuments();
  RequireSuccessEnvelope(listed_after);
  const auto restored_pages = listed_after.value(QStringLiteral("result")).toList();
  Require(restored_pages.size() == 1, "the restored page should be back in listDocuments");
  Require(restored_pages.front().toMap().value(QStringLiteral("id")).toString() == page_id,
          "restore must return the originally deleted page rather than a replacement page");

  const auto trash_after = bridge.listTrash();
  RequireSuccessEnvelope(trash_after);
  Require(trash_after.value(QStringLiteral("result")).toList().isEmpty(),
          "the restored page should no longer appear in listTrash");
}

auto TestPermanentlyDeleteDocumentRemovesItForGood() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto listed_before = bridge.listDocuments();
  RequireSuccessEnvelope(listed_before);
  const auto page_id = listed_before.value(QStringLiteral("result"))
                           .toList()
                           .front()
                           .toMap()
                           .value(QStringLiteral("id"))
                           .toString();

  RequireSuccessEnvelope(bridge.deleteDocument(page_id));
  RequireSuccessEnvelope(bridge.permanentlyDeleteDocument(page_id));

  const auto trash_after = bridge.listTrash();
  RequireSuccessEnvelope(trash_after);
  Require(trash_after.value(QStringLiteral("result")).toList().isEmpty(),
          "the permanently-deleted page should no longer appear in listTrash");

  const auto loaded = bridge.loadDocument(page_id);
  Require(!loaded.value(QStringLiteral("ok")).toBool(),
          "loading a permanently-deleted page should fail");
}

auto TestEmptyTrashRemovesAllCurrentlyTrashedDocuments() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto first = bridge.createDocument();
  RequireSuccessEnvelope(first);
  const auto first_id =
      first.value(QStringLiteral("result")).toMap().value(QStringLiteral("id")).toString();
  const auto second = bridge.createDocument();
  RequireSuccessEnvelope(second);
  const auto second_id =
      second.value(QStringLiteral("result")).toMap().value(QStringLiteral("id")).toString();

  RequireSuccessEnvelope(bridge.deleteDocument(first_id));
  RequireSuccessEnvelope(bridge.deleteDocument(second_id));

  const auto trash_before = bridge.listTrash();
  RequireSuccessEnvelope(trash_before);
  Require(trash_before.value(QStringLiteral("result")).toList().size() == 2,
          "both deleted pages should be in the trash before emptying it");

  RequireSuccessEnvelope(bridge.emptyTrash());

  const auto trash_after = bridge.listTrash();
  RequireSuccessEnvelope(trash_after);
  Require(trash_after.value(QStringLiteral("result")).toList().isEmpty(),
          "the trash should be empty after emptyTrash");
}

auto TestCreateJupyterNotebookProducesLoadableNbformatContent() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto created = bridge.createDocumentInWorkspace(QStringLiteral("default"),
                                                        QStringLiteral("jupyterNotebook"));
  RequireSuccessEnvelope(created);
  const auto created_result = created.value(QStringLiteral("result")).toMap();
  Require(
      created_result.value(QStringLiteral("kind")).toString() == QStringLiteral("jupyterNotebook"),
      "created document should report kind=jupyterNotebook");
  const auto page_id = created_result.value(QStringLiteral("id")).toString();

  const auto loaded = bridge.loadDocument(page_id);
  RequireSuccessEnvelope(loaded);
  const auto loaded_result = loaded.value(QStringLiteral("result")).toMap();
  const auto raw_content = loaded_result.value(QStringLiteral("rawContent")).toString();
  Require(!raw_content.isEmpty(), "loadDocument should return non-empty rawContent for a notebook");

  const auto parsed = QJsonDocument::fromJson(raw_content.toUtf8());
  Require(parsed.isObject(), "rawContent must be valid JSON for a freshly-created notebook");
  Require(parsed.object().value(QStringLiteral("cells")).isArray(),
          "freshly-created notebook's rawContent must have a 'cells' array (nbformat v4 shape)");
}

auto TestCreateExcalidrawCanvasProducesLoadableSceneContent() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto created = bridge.createDocumentInWorkspace(QStringLiteral("default"),
                                                        QStringLiteral("excalidrawCanvas"));
  RequireSuccessEnvelope(created);
  const auto created_result = created.value(QStringLiteral("result")).toMap();
  Require(
      created_result.value(QStringLiteral("kind")).toString() == QStringLiteral("excalidrawCanvas"),
      "created document should report kind=excalidrawCanvas");
  const auto page_id = created_result.value(QStringLiteral("id")).toString();

  const auto loaded = bridge.loadDocument(page_id);
  RequireSuccessEnvelope(loaded);
  const auto loaded_result = loaded.value(QStringLiteral("result")).toMap();
  const auto raw_content = loaded_result.value(QStringLiteral("rawContent")).toString();
  Require(!raw_content.isEmpty(), "loadDocument should return non-empty rawContent for a canvas");

  const auto parsed = QJsonDocument::fromJson(raw_content.toUtf8());
  Require(parsed.isObject(), "rawContent must be valid JSON for a freshly-created canvas");
  Require(parsed.object().value(QStringLiteral("type")).toString() == QStringLiteral("excalidraw"),
          "freshly-created canvas's rawContent must have type=\"excalidraw\" (Excalidraw scene "
          "shape), not the BlockNote snapshot shape");
  Require(parsed.object().value(QStringLiteral("elements")).isArray(),
          "freshly-created canvas's rawContent must have an 'elements' array (Excalidraw scene "
          "shape)");
}

// Mirrors NotebookView.tsx's scheduleSave(): edit a cell, call updateSnapshot() with the whole
// notebook object, then reload and check the edit persisted and the content is still valid JSON.
auto TestUpdateSnapshotRoundTripsForJupyterNotebook() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto created = bridge.createDocumentInWorkspace(QStringLiteral("default"),
                                                        QStringLiteral("jupyterNotebook"));
  RequireSuccessEnvelope(created);
  const auto page_id =
      created.value(QStringLiteral("result")).toMap().value(QStringLiteral("id")).toString();
  RequireSuccessEnvelope(bridge.loadDocument(page_id));

  const auto edited = bridge.updateSnapshot(page_id, QStringLiteral(R"({
    "cells": [
      { "cell_type": "markdown", "source": ["Edited from test"], "metadata": {} }
    ],
    "metadata": {},
    "nbformat": 4,
    "nbformat_minor": 5
  })"));
  RequireSuccessEnvelope(edited);

  const auto reloaded = bridge.loadDocument(page_id);
  RequireSuccessEnvelope(reloaded);
  const auto raw_content = reloaded.value(QStringLiteral("result"))
                               .toMap()
                               .value(QStringLiteral("rawContent"))
                               .toString();

  const auto parsed = QJsonDocument::fromJson(raw_content.toUtf8());
  Require(parsed.isObject(), "rawContent must still be valid JSON after an edit+save");
  const auto cells = parsed.object().value(QStringLiteral("cells")).toArray();
  Require(cells.size() == 1, "edited cell should persist through updateSnapshot -> loadDocument");
}

// Mirrors ExcalidrawCanvasView.tsx's debounced onChange: edit the scene, call updateSnapshot()
// with the whole scene object, then reload and check the edit persisted.
auto TestUpdateSnapshotRoundTripsForExcalidrawCanvas() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto created = bridge.createDocumentInWorkspace(QStringLiteral("default"),
                                                        QStringLiteral("excalidrawCanvas"));
  RequireSuccessEnvelope(created);
  const auto page_id =
      created.value(QStringLiteral("result")).toMap().value(QStringLiteral("id")).toString();
  RequireSuccessEnvelope(bridge.loadDocument(page_id));

  const auto edited = bridge.updateSnapshot(page_id, QStringLiteral(R"({
    "type": "excalidraw",
    "version": 2,
    "elements": [{ "id": "el1", "type": "rectangle" }],
    "appState": { "viewBackgroundColor": "#ffffff" },
    "files": {}
  })"));
  RequireSuccessEnvelope(edited);

  const auto reloaded = bridge.loadDocument(page_id);
  RequireSuccessEnvelope(reloaded);
  const auto raw_content = reloaded.value(QStringLiteral("result"))
                               .toMap()
                               .value(QStringLiteral("rawContent"))
                               .toString();

  const auto parsed = QJsonDocument::fromJson(raw_content.toUtf8());
  Require(parsed.isObject(), "rawContent must still be valid JSON after an edit+save");
  const auto elements = parsed.object().value(QStringLiteral("elements")).toArray();
  Require(elements.size() == 1,
          "edited element should persist through updateSnapshot -> loadDocument");
}

auto TestOpenDocumentReturnsLoadedDocument() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto list_response = bridge.listDocuments();
  RequireSuccessEnvelope(list_response);
  const auto page_id = list_response.value(QStringLiteral("result"))
                           .toList()
                           .front()
                           .toMap()
                           .value(QStringLiteral("id"))
                           .toString();

  const auto response = bridge.openDocument(page_id);
  RequireSuccessEnvelope(response);

  const auto document = response.value(QStringLiteral("result")).toMap();
  Require(document.value(QStringLiteral("id")).toString() == page_id,
          "opened document id should match selected page");
  Require(document.value(QStringLiteral("blocks")).toList().size() == 2,
          "opened welcome document should include blocks");
}

auto TestWorkspaceListIsolation() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  RequireSuccessEnvelope(bridge.createDocument());

  bridge.SetCurrentWorkspaceId(QStringLiteral("team-b"));
  const auto team_b_list = bridge.listDocuments();
  RequireSuccessEnvelope(team_b_list);
  const auto team_b_pages = team_b_list.value(QStringLiteral("result")).toList();
  Require(team_b_pages.size() == 1, "new workspace should bootstrap its own welcome page");
  Require(team_b_pages.front().toMap().value(QStringLiteral("workspaceId")).toString() ==
              QStringLiteral("team-b"),
          "bootstrapped page should belong to active workspace");

  bridge.SetCurrentWorkspaceId(QStringLiteral("default"));
  const auto default_list = bridge.listDocuments();
  RequireSuccessEnvelope(default_list);
  const auto default_pages = default_list.value(QStringLiteral("result")).toList();
  Require(!default_pages.isEmpty(), "default workspace should still have its own documents");
  for (const auto& page_value : default_pages) {
    Require(page_value.toMap().value(QStringLiteral("workspaceId")).toString() ==
                QStringLiteral("default"),
            "listDocuments should only return documents from active workspace");
  }
}

auto TestEmptyRepositoryWithRemoteSyncExpectedSkipsWelcome() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  FakeSyncStateProvider sync_provider(QStringList{QStringLiteral("default")});
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);
  bridge.SetSyncStateProvider(&sync_provider);

  const auto response = bridge.listDocuments();
  RequireSuccessEnvelope(response);

  const auto pages = response.value(QStringLiteral("result")).toList();
  Require(pages.isEmpty(),
          "empty repository with expected remote sync should not bootstrap a welcome page");
}

auto TestEmptyRepositoryWithUnreadySyncStillBootstrapsWelcome() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  FakeSyncStateProvider sync_provider;
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);
  bridge.SetSyncStateProvider(&sync_provider);

  const auto response = bridge.listDocuments();
  RequireSuccessEnvelope(response);

  const auto pages = response.value(QStringLiteral("result")).toList();
  Require(pages.size() == 1,
          "empty repository without confirmed remote sync should bootstrap a welcome page");
}

auto TestNonEmptyRepositoryWithRemoteSyncExpectedReturnsDocuments() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  FakeSyncStateProvider sync_provider(QStringList{QStringLiteral("default")});
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);
  bridge.SetSyncStateProvider(&sync_provider);

  const auto created = bridge.createDocument();
  RequireSuccessEnvelope(created);

  const auto response = bridge.listDocuments();
  RequireSuccessEnvelope(response);

  const auto pages = response.value(QStringLiteral("result")).toList();
  Require(pages.size() == 1,
          "non-empty repository with expected remote sync should return existing documents");
}

auto TestWorkspaceMismatchBlocksCrossWorkspaceLoad() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto created = bridge.createDocument();
  RequireSuccessEnvelope(created);
  const auto created_id =
      created.value(QStringLiteral("result")).toMap().value(QStringLiteral("id")).toString();

  bridge.SetCurrentWorkspaceId(QStringLiteral("team-b"));
  const auto loaded = bridge.loadDocument(created_id);
  RequireErrorEnvelope(loaded, QStringLiteral("workspace_mismatch"));
}

auto TestSessionContextOverridesWorkspaceAndAuthor() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);
  bridge.SetCurrentAuthorId(QStringLiteral("subject-42"));
  bridge.SetCurrentWorkspaceId(QStringLiteral("workspace-blue"));

  const auto created = bridge.createDocument();
  RequireSuccessEnvelope(created);
  const auto result = created.value(QStringLiteral("result")).toMap();
  Require(
      result.value(QStringLiteral("workspaceId")).toString() == QStringLiteral("workspace-blue"),
      "created document should use current workspace from session context");
  Require(result.value(QStringLiteral("createdBy")).toString() == QStringLiteral("subject-42"),
          "created document should use current author from session context");

  const auto loaded = bridge.loadDocument(result.value(QStringLiteral("id")).toString());
  RequireSuccessEnvelope(loaded);
  const auto loaded_result = loaded.value(QStringLiteral("result")).toMap();
  Require(loaded_result.value(QStringLiteral("workspaceId")).toString() ==
              QStringLiteral("workspace-blue"),
          "loaded document should preserve session-derived workspace");
  Require(
      loaded_result.value(QStringLiteral("createdBy")).toString() == QStringLiteral("subject-42"),
      "loaded document should preserve session-derived author");
}

auto TestValidSnapshot() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);
  const auto list_response = bridge.listDocuments();
  RequireSuccessEnvelope(list_response);
  const auto page_id = list_response.value(QStringLiteral("result"))
                           .toList()
                           .front()
                           .toMap()
                           .value(QStringLiteral("id"))
                           .toString();

  RequireSuccessEnvelope(bridge.loadDocument(page_id));

  const auto response = bridge.updateSnapshot(page_id, QStringLiteral(R"([
    {
      "id": "b1",
      "type": "paragraph",
      "content": [
        { "type": "text", "text": "Saved from test", "styles": {} }
      ],
      "children": []
    },
    {
      "id": "quote-1",
      "type": "quote",
      "content": [
        { "type": "text", "text": "Quoted from bridge test", "styles": {} }
      ],
      "children": []
    }
  ])"));

  RequireSuccessEnvelope(response);
}

auto HeadingSnapshot(const QString& heading_text) -> QString {
  return QStringLiteral(R"([
    {
      "id": "heading-1",
      "type": "heading",
      "props": { "level": 1 },
      "content": [ { "type": "text", "text": "%1", "styles": {} } ],
      "children": []
    }
  ])")
      .arg(heading_text);
}

// Issue #166: each successful save that actually changes content should record the content it's
// about to overwrite as a revision -- so after two edits, the pre-edit-1 and pre-edit-2 states
// should both be recoverable, distinguishable by title (each edit sets a distinct h1 heading).
auto TestUpdateSnapshotRecordsRevisionsAndRestoreBringsBackOldContent() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto list_response = bridge.listDocuments();
  RequireSuccessEnvelope(list_response);
  const auto page_id = list_response.value(QStringLiteral("result"))
                           .toList()
                           .front()
                           .toMap()
                           .value(QStringLiteral("id"))
                           .toString();

  RequireSuccessEnvelope(bridge.loadDocument(page_id));
  RequireSuccessEnvelope(
      bridge.updateSnapshot(page_id, HeadingSnapshot(QStringLiteral("Edit One"))));
  RequireSuccessEnvelope(
      bridge.updateSnapshot(page_id, HeadingSnapshot(QStringLiteral("Edit Two"))));

  const auto revisions_after_two_edits = bridge.listDocumentRevisions(page_id);
  RequireSuccessEnvelope(revisions_after_two_edits);
  const auto revisions_list = revisions_after_two_edits.value(QStringLiteral("result")).toList();
  Require(revisions_list.size() == 2,
          "two content-changing saves should have recorded two revisions (the welcome page's "
          "original content, then Edit One's content)");

  QString edit_one_revision_id;
  for (const auto& revision_variant : revisions_list) {
    const auto revision = revision_variant.toMap();
    if (revision.value(QStringLiteral("title")).toString() == QStringLiteral("Edit One")) {
      edit_one_revision_id = revision.value(QStringLiteral("id")).toString();
    }
  }
  Require(!edit_one_revision_id.isEmpty(),
          "the revision recorded just before Edit Two should carry Edit One's title");

  const auto restored = bridge.restoreDocumentRevision(page_id, edit_one_revision_id);
  RequireSuccessEnvelope(restored);
  Require(
      restored.value(QStringLiteral("result")).toMap().value(QStringLiteral("title")).toString() ==
          QStringLiteral("Edit One"),
      "restoring a revision should return the restored document's metadata");

  const auto reloaded = bridge.loadDocument(page_id);
  RequireSuccessEnvelope(reloaded);
  Require(
      reloaded.value(QStringLiteral("result")).toMap().value(QStringLiteral("title")).toString() ==
          QStringLiteral("Edit One"),
      "the document's live content should be Edit One's content again after restore");

  // Restoring must itself be undoable: it should have recorded Edit Two's content (the state it
  // just overwrote) as a new revision, on top of the two already there.
  const auto revisions_after_restore = bridge.listDocumentRevisions(page_id);
  RequireSuccessEnvelope(revisions_after_restore);
  Require(revisions_after_restore.value(QStringLiteral("result")).toList().size() == 3,
          "restoring a revision should record the just-overwritten content as a new revision, "
          "not delete the revision it restored from");
}

auto TestInvalidJsonSnapshot() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);
  const auto list_response = bridge.listDocuments();
  RequireSuccessEnvelope(list_response);
  const auto page_id = list_response.value(QStringLiteral("result"))
                           .toList()
                           .front()
                           .toMap()
                           .value(QStringLiteral("id"))
                           .toString();
  RequireSuccessEnvelope(bridge.loadDocument(page_id));

  const auto response = bridge.updateSnapshot(page_id, QStringLiteral("{"));

  RequireErrorEnvelope(response, QStringLiteral("invalid_json"));
}

auto TestInvalidRootSnapshot() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);
  const auto list_response = bridge.listDocuments();
  RequireSuccessEnvelope(list_response);
  const auto page_id = list_response.value(QStringLiteral("result"))
                           .toList()
                           .front()
                           .toMap()
                           .value(QStringLiteral("id"))
                           .toString();
  RequireSuccessEnvelope(bridge.loadDocument(page_id));

  const auto response =
      bridge.updateSnapshot(page_id, QStringLiteral(R"({ "type": "paragraph" })"));

  RequireErrorEnvelope(response, QStringLiteral("missing_schema_version"));
}

auto TestRenameDocumentRejectedWhenCurrentDocumentLocked() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto listed = bridge.listDocuments();
  RequireSuccessEnvelope(listed);
  const auto page_id = listed.value(QStringLiteral("result"))
                           .toList()
                           .front()
                           .toMap()
                           .value(QStringLiteral("id"))
                           .toString();

  RequireSuccessEnvelope(bridge.openDocument(page_id));
  bridge.SetCurrentDocumentAccess(false, QStringLiteral("someone-else"),
                                  QStringLiteral("Locked by someone else."));

  const auto response = bridge.renameDocument(page_id, QStringLiteral("Should not apply"));
  RequireErrorEnvelope(response, QStringLiteral("document_read_only"));

  const auto reloaded = bridge.loadDocument(page_id);
  RequireSuccessEnvelope(reloaded);
  Require(
      reloaded.value(QStringLiteral("result")).toMap().value(QStringLiteral("title")).toString() !=
          QStringLiteral("Should not apply"),
      "rename must not apply while the current document is locked/read-only");
}

auto TestRenameDocumentSucceedsWhenCurrentDocumentEditable() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto listed = bridge.listDocuments();
  RequireSuccessEnvelope(listed);
  const auto page_id = listed.value(QStringLiteral("result"))
                           .toList()
                           .front()
                           .toMap()
                           .value(QStringLiteral("id"))
                           .toString();

  RequireSuccessEnvelope(bridge.openDocument(page_id));
  bridge.SetCurrentDocumentAccess(true, QString{}, QString{});

  const auto response = bridge.renameDocument(page_id, QStringLiteral("Editable rename"));
  RequireSuccessEnvelope(response);
  Require(
      response.value(QStringLiteral("result")).toMap().value(QStringLiteral("title")).toString() ==
          QStringLiteral("Editable rename"),
      "rename should still succeed when the current document is editable");
}

auto TestUpdateDocumentPlacementRejectedWhenCurrentDocumentLocked() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto listed = bridge.listDocuments();
  RequireSuccessEnvelope(listed);
  const auto page_id = listed.value(QStringLiteral("result"))
                           .toList()
                           .front()
                           .toMap()
                           .value(QStringLiteral("id"))
                           .toString();

  RequireSuccessEnvelope(bridge.openDocument(page_id));
  bridge.SetCurrentDocumentAccess(false, QStringLiteral("someone-else"),
                                  QStringLiteral("Locked by someone else."));

  const auto response = bridge.updateDocumentPlacement(page_id, QString{}, false, 5);
  RequireErrorEnvelope(response, QStringLiteral("document_read_only"));
}

auto TestUpdateDocumentPlacementSucceedsWhenCurrentDocumentEditable() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto listed = bridge.listDocuments();
  RequireSuccessEnvelope(listed);
  const auto page_id = listed.value(QStringLiteral("result"))
                           .toList()
                           .front()
                           .toMap()
                           .value(QStringLiteral("id"))
                           .toString();

  RequireSuccessEnvelope(bridge.openDocument(page_id));
  bridge.SetCurrentDocumentAccess(true, QString{}, QString{});

  const auto response = bridge.updateDocumentPlacement(page_id, QString{}, false, 5);
  RequireSuccessEnvelope(response);
  Require(
      response.value(QStringLiteral("result")).toMap().value(QStringLiteral("sortOrder")).toInt() ==
          5,
      "placement update should still succeed when the current document is editable");
}

auto TestDeleteDocumentRejectedWhenCurrentDocumentLocked() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto created = bridge.createDocument();
  RequireSuccessEnvelope(created);
  const auto created_id =
      created.value(QStringLiteral("result")).toMap().value(QStringLiteral("id")).toString();

  RequireSuccessEnvelope(bridge.openDocument(created_id));
  bridge.SetCurrentDocumentAccess(false, QStringLiteral("someone-else"),
                                  QStringLiteral("Locked by someone else."));

  const auto response = bridge.deleteDocument(created_id);
  RequireErrorEnvelope(response, QStringLiteral("document_read_only"));

  const auto reloaded = bridge.loadDocument(created_id);
  RequireSuccessEnvelope(reloaded);
}

auto TestRenameDocumentRejectedWhenCurrentDocumentConflicted() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto listed = bridge.listDocuments();
  RequireSuccessEnvelope(listed);
  const auto page_id = listed.value(QStringLiteral("result"))
                           .toList()
                           .front()
                           .toMap()
                           .value(QStringLiteral("id"))
                           .toString();

  RequireSuccessEnvelope(bridge.openDocument(page_id));
  bridge.SetCurrentDocumentConflicted(true);

  const auto response = bridge.renameDocument(page_id, QStringLiteral("Should not apply"));
  RequireErrorEnvelope(response, QStringLiteral("document_read_only"));

  const auto reloaded = bridge.loadDocument(page_id);
  RequireSuccessEnvelope(reloaded);
  Require(
      reloaded.value(QStringLiteral("result")).toMap().value(QStringLiteral("title")).toString() !=
          QStringLiteral("Should not apply"),
      "rename must not apply while the current document has an unresolved conflict");
}

auto TestUpdateDocumentPlacementRejectedWhenCurrentDocumentConflicted() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto listed = bridge.listDocuments();
  RequireSuccessEnvelope(listed);
  const auto page_id = listed.value(QStringLiteral("result"))
                           .toList()
                           .front()
                           .toMap()
                           .value(QStringLiteral("id"))
                           .toString();

  RequireSuccessEnvelope(bridge.openDocument(page_id));
  bridge.SetCurrentDocumentConflicted(true);

  const auto response = bridge.updateDocumentPlacement(page_id, QString{}, false, 5);
  RequireErrorEnvelope(response, QStringLiteral("document_read_only"));
}

auto TestDeleteDocumentRejectedWhenCurrentDocumentConflicted() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto created = bridge.createDocument();
  RequireSuccessEnvelope(created);
  const auto created_id =
      created.value(QStringLiteral("result")).toMap().value(QStringLiteral("id")).toString();

  RequireSuccessEnvelope(bridge.openDocument(created_id));
  bridge.SetCurrentDocumentConflicted(true);

  const auto response = bridge.deleteDocument(created_id);
  RequireErrorEnvelope(response, QStringLiteral("document_read_only"));

  const auto reloaded = bridge.loadDocument(created_id);
  RequireSuccessEnvelope(reloaded);
}

auto TestUpdateSnapshotRejectedWhenCurrentDocumentConflicted() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto listed = bridge.listDocuments();
  RequireSuccessEnvelope(listed);
  const auto page_id = listed.value(QStringLiteral("result"))
                           .toList()
                           .front()
                           .toMap()
                           .value(QStringLiteral("id"))
                           .toString();
  RequireSuccessEnvelope(bridge.loadDocument(page_id));
  bridge.SetCurrentDocumentConflicted(true);

  const auto response = bridge.updateSnapshot(page_id, QStringLiteral(R"([
    {
      "id": "b1",
      "type": "paragraph",
      "content": [
        { "type": "text", "text": "Should not save", "styles": {} }
      ],
      "children": []
    }
  ])"));

  RequireErrorEnvelope(response, QStringLiteral("document_read_only"));
}

auto TestConflictFlagClearsOnFreshLoad() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto listed = bridge.listDocuments();
  RequireSuccessEnvelope(listed);
  const auto page_id = listed.value(QStringLiteral("result"))
                           .toList()
                           .front()
                           .toMap()
                           .value(QStringLiteral("id"))
                           .toString();

  RequireSuccessEnvelope(bridge.openDocument(page_id));
  bridge.SetCurrentDocumentConflicted(true);
  RequireErrorEnvelope(bridge.renameDocument(page_id, QStringLiteral("nope")),
                       QStringLiteral("document_read_only"));

  // Reloading the same document resets the conflict flag until the caller
  // re-applies it (mirrors how the lock's pending/current state resets).
  RequireSuccessEnvelope(bridge.loadDocument(page_id));
  const auto response = bridge.renameDocument(page_id, QStringLiteral("Now editable"));
  RequireSuccessEnvelope(response);
}

auto TestDeleteDocumentSucceedsWhenCurrentDocumentEditable() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto created = bridge.createDocument();
  RequireSuccessEnvelope(created);
  const auto created_id =
      created.value(QStringLiteral("result")).toMap().value(QStringLiteral("id")).toString();

  RequireSuccessEnvelope(bridge.openDocument(created_id));
  bridge.SetCurrentDocumentAccess(true, QString{}, QString{});

  const auto response = bridge.deleteDocument(created_id);
  RequireSuccessEnvelope(response);
}

auto TestStartAiRequestReturnsUniqueRequestIds() -> void {
  cppwiki::bridge::QEditorBridge bridge;

  const auto first_id =
      bridge.startAiRequest(QStringLiteral("Make this punchier"),
                            QStringLiteral("Some paragraph text."), QStringLiteral("rewrite"));
  const auto second_id =
      bridge.startAiRequest(QStringLiteral("Continue writing"),
                            QStringLiteral("Some paragraph text."), QStringLiteral("autocomplete"));

  Require(!first_id.isEmpty(), "startAiRequest must return a non-empty request id");
  Require(!second_id.isEmpty(), "startAiRequest must return a non-empty request id");
  Require(first_id != second_id, "each startAiRequest call must return a distinct request id");
}

// Issue #65: startAiRequest() accepts an optional tool name + JSON Schema so
// xl-ai's structured tool-call requests can be forwarded through the bridge.
// This does not exercise the network call (no backend/key store is
// configured in this test, matching TestStartAiRequestReturnsUniqueRequestIds
// above), just that the overload accepting tool arguments compiles, is
// invokable, and still returns a valid, distinct request id.
auto TestStartAiRequestWithToolSchemaReturnsRequestId() -> void {
  cppwiki::bridge::QEditorBridge bridge;

  const auto request_id =
      bridge.startAiRequest(QStringLiteral("Add a heading"), QStringLiteral("Some paragraph text."),
                            QStringLiteral("rewrite"), QStringLiteral("applyDocumentOperations"),
                            QStringLiteral(R"({"type":"array","items":{"type":"object"}})"));

  Require(!request_id.isEmpty(),
          "startAiRequest with a tool schema must still return a non-empty request id");
}

auto TestAttachmentUploadPersistsOnlyAfterComplete() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);
  bridge.SetCurrentWorkspaceId(QStringLiteral("engineering"));
  bridge.SetCurrentAuthorId(QStringLiteral("tester"));

  const auto begun = bridge.beginAttachmentUpload(QVariantMap{
      {QStringLiteral("filename"), QStringLiteral("architecture.png")},
      {QStringLiteral("mimeType"), QStringLiteral("image/png")},
      {QStringLiteral("sizeBytes"), 4},
  });
  RequireSuccessEnvelope(begun);
  const auto upload_id =
      begun.value(QStringLiteral("result")).toMap().value(QStringLiteral("uploadId")).toString();
  Require(!upload_id.isEmpty(), "upload must return an ID");

  RequireSuccessEnvelope(bridge.appendAttachmentChunk(
      upload_id, QString::fromLatin1(QByteArray::fromHex("8950").toBase64())));
  RequireSuccessEnvelope(bridge.appendAttachmentChunk(
      upload_id, QString::fromLatin1(QByteArray::fromHex("4e47").toBase64())));
  const auto completed = bridge.completeAttachmentUpload(upload_id);
  RequireSuccessEnvelope(completed);
  const auto uri =
      completed.value(QStringLiteral("result")).toMap().value(QStringLiteral("uri")).toString();
  Require(uri.startsWith(QStringLiteral("cppwiki-attachment://")),
          "completed upload must return a private URI");

  const auto attachment_id = uri.mid(QStringLiteral("cppwiki-attachment://").size());
  const auto stored = repository->LoadAttachment(attachment_id.toStdString(), "engineering");
  Require(stored.attachment.has_value() &&
              stored.attachment->bytes == std::vector<std::uint8_t>({0x89, 0x50, 0x4E, 0x47}),
          "completed upload must persist accumulated bytes");
  RequireErrorEnvelope(bridge.appendAttachmentChunk(upload_id, QStringLiteral("eA==")),
                       QStringLiteral("upload_not_found"));

  const auto oversize = bridge.beginAttachmentUpload(QVariantMap{
      {QStringLiteral("filename"), QStringLiteral("large.bin")},
      {QStringLiteral("mimeType"), QStringLiteral("application/octet-stream")},
      {QStringLiteral("sizeBytes"), static_cast<qlonglong>(26LL * 1024LL * 1024LL)},
  });
  RequireErrorEnvelope(oversize, QStringLiteral("attachment_too_large"));
}

}  // namespace

auto main() -> int {
  TestBridgeInfo();
  TestInitialDocumentStartsEmpty();
  TestDocumentListBootstrapsWelcomePage();
  TestCreateDocument();
  TestCreateDocumentLoadsEmptyAndSaves();
  TestCreateDocumentDoesNotHijackAutosaveSelection();
  TestRenameDocumentUpdatesTitle();
  TestRenameDocumentRejectedWhenCurrentDocumentLocked();
  TestRenameDocumentSucceedsWhenCurrentDocumentEditable();
  TestUpdateDocumentPlacementRejectedWhenCurrentDocumentLocked();
  TestUpdateDocumentPlacementSucceedsWhenCurrentDocumentEditable();
  TestDeleteDocumentRemovesItFromList();
  TestDeleteDocumentFromPreRootWorkspaceLeavesNormalListEmpty();
  TestDeleteDocumentMovesItToTrashInsteadOfErasingIt();
  TestRestoreDocumentBringsItBackToTheNormalList();
  TestPermanentlyDeleteDocumentRemovesItForGood();
  TestEmptyTrashRemovesAllCurrentlyTrashedDocuments();
  TestDeleteDocumentRejectedWhenCurrentDocumentLocked();
  TestDeleteDocumentSucceedsWhenCurrentDocumentEditable();
  TestRenameDocumentRejectedWhenCurrentDocumentConflicted();
  TestUpdateDocumentPlacementRejectedWhenCurrentDocumentConflicted();
  TestDeleteDocumentRejectedWhenCurrentDocumentConflicted();
  TestUpdateSnapshotRejectedWhenCurrentDocumentConflicted();
  TestConflictFlagClearsOnFreshLoad();
  TestCreateJupyterNotebookProducesLoadableNbformatContent();
  TestCreateExcalidrawCanvasProducesLoadableSceneContent();
  TestUpdateSnapshotRoundTripsForJupyterNotebook();
  TestUpdateSnapshotRoundTripsForExcalidrawCanvas();
  TestOpenDocumentReturnsLoadedDocument();
  TestWorkspaceListIsolation();
  TestEmptyRepositoryWithRemoteSyncExpectedSkipsWelcome();
  TestEmptyRepositoryWithUnreadySyncStillBootstrapsWelcome();
  TestNonEmptyRepositoryWithRemoteSyncExpectedReturnsDocuments();
  TestWorkspaceMismatchBlocksCrossWorkspaceLoad();
  TestSessionContextOverridesWorkspaceAndAuthor();
  TestValidSnapshot();
  TestUpdateSnapshotRecordsRevisionsAndRestoreBringsBackOldContent();
  TestInvalidJsonSnapshot();
  TestInvalidRootSnapshot();
  TestStartAiRequestReturnsUniqueRequestIds();
  TestStartAiRequestWithToolSchemaReturnsRequestId();
  TestAttachmentUploadPersistsOnlyAfterComplete();

  spdlog::info("cppwiki_bridge_tests passed");
  return EXIT_SUCCESS;
}
