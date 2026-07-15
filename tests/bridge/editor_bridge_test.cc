#include "bridge/editor_bridge.h"

#include <spdlog/spdlog.h>

#include <QVariant>
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

  [[nodiscard]] auto SupportsSync() const -> bool override {
    return true;
  }

 private:
  std::map<std::string, cppwiki::storage::DocumentRecord> documents_;
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

  const auto saved = bridge.updateSnapshot(QStringLiteral(R"([
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

  const auto saved = bridge.updateSnapshot(QStringLiteral(R"([
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

  const auto created = bridge.createDocument();
  RequireSuccessEnvelope(created);
  const auto created_id =
      created.value(QStringLiteral("result")).toMap().value(QStringLiteral("id")).toString();

  const auto deleted = bridge.deleteDocument(created_id);
  RequireSuccessEnvelope(deleted);

  const auto listed = bridge.listDocuments();
  RequireSuccessEnvelope(listed);
  const auto pages = listed.value(QStringLiteral("result")).toList();
  Require(pages.size() == 1, "after deleting only note, welcome page should be bootstrapped again");
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

  const auto response = bridge.updateSnapshot(QStringLiteral(R"([
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

  const auto response = bridge.updateSnapshot(QStringLiteral("{"));

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

  const auto response = bridge.updateSnapshot(QStringLiteral(R"({ "type": "paragraph" })"));

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

  const auto response = bridge.updateSnapshot(QStringLiteral(R"([
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
  TestDeleteDocumentRejectedWhenCurrentDocumentLocked();
  TestDeleteDocumentSucceedsWhenCurrentDocumentEditable();
  TestRenameDocumentRejectedWhenCurrentDocumentConflicted();
  TestUpdateDocumentPlacementRejectedWhenCurrentDocumentConflicted();
  TestDeleteDocumentRejectedWhenCurrentDocumentConflicted();
  TestUpdateSnapshotRejectedWhenCurrentDocumentConflicted();
  TestConflictFlagClearsOnFreshLoad();
  TestOpenDocumentReturnsLoadedDocument();
  TestWorkspaceListIsolation();
  TestEmptyRepositoryWithRemoteSyncExpectedSkipsWelcome();
  TestEmptyRepositoryWithUnreadySyncStillBootstrapsWelcome();
  TestNonEmptyRepositoryWithRemoteSyncExpectedReturnsDocuments();
  TestWorkspaceMismatchBlocksCrossWorkspaceLoad();
  TestSessionContextOverridesWorkspaceAndAuthor();
  TestValidSnapshot();
  TestInvalidJsonSnapshot();
  TestInvalidRootSnapshot();

  spdlog::info("cppwiki_bridge_tests passed");
  return EXIT_SUCCESS;
}
