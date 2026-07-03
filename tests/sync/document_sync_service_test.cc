#include "sync/document_sync_service.h"

#include <spdlog/spdlog.h>

#include <QCoreApplication>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string_view>

#include "app/program_settings.h"
#include "core/constants.h"
#include "core/qt_string.h"
#include "storage/local_document_repository.h"

#ifdef CPPWIKI_ENABLE_CBLITE_STORAGE
#include "storage/cblite_document_repository.h"
#endif

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto MakeUniqueAppDataDirectory() -> QString {
  static int counter = 0;
  const auto path = std::filesystem::temp_directory_path() /
                    ("cppwiki-sync-service-test-" + std::to_string(counter++));
  std::filesystem::remove_all(path);
  return QString::fromStdString(path.string());
}

class FakeDocumentRepository final : public cppwiki::storage::LocalDocumentRepository {
 public:
  [[nodiscard]] auto SaveDocument(const cppwiki::storage::DocumentRecord&)
      -> cppwiki::storage::SaveDocumentResult override {
    return {};
  }

  [[nodiscard]] auto DeleteDocument(std::string_view)
      -> cppwiki::storage::DeleteDocumentResult override {
    return {};
  }

  [[nodiscard]] auto LoadDocument(std::string_view)
      -> cppwiki::storage::LoadDocumentResult override {
    return {};
  }

  [[nodiscard]] auto ListDocuments() -> cppwiki::storage::ListDocumentsResult override {
    return {
        .documents = listed_documents,
        .error = std::nullopt,
    };
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

  [[nodiscard]] auto SetSyncAccessToken(std::string access_token)
      -> cppwiki::storage::SyncOperationResult override {
    access_token_set = true;
    last_access_token = std::move(access_token);
    return {};
  }

  [[nodiscard]] auto ApplySyncBootstrap(const cppwiki::sync::SyncBootstrap& bootstrap)
      -> cppwiki::storage::SyncOperationResult override {
    bootstrap_applied = true;
    last_bootstrap = bootstrap;
    sync_status = {
        .state = cppwiki::storage::SyncLifecycleState::kConfigured,
        .status_text = "Sync bootstrap configured",
        .has_conflicts = sync_status.has_conflicts,
        .conflict_count = sync_status.conflict_count,
    };
    return {};
  }

  [[nodiscard]] auto StartSync() -> cppwiki::storage::SyncOperationResult override {
    start_sync_called = true;
    sync_status = {
        .state = cppwiki::storage::SyncLifecycleState::kRunning,
        .status_text = "Sync running",
        .initial_pull_active = true,
        .initial_pull_completed = false,
        .has_conflicts = sync_status.has_conflicts,
        .conflict_count = sync_status.conflict_count,
    };
    return {};
  }

  [[nodiscard]] auto StopSync() -> cppwiki::storage::SyncOperationResult override {
    stop_sync_called = true;
    sync_status = {
        .state = cppwiki::storage::SyncLifecycleState::kConfigured,
        .status_text = "Sync stopped",
        .has_conflicts = sync_status.has_conflicts,
        .conflict_count = sync_status.conflict_count,
    };
    return {};
  }

  [[nodiscard]] auto GetSyncStatus() const -> cppwiki::storage::SyncStatus override {
    return sync_status;
  }

  [[nodiscard]] auto SaveWorkspaceRoot(const cppwiki::storage::WorkspaceRootRecord& record)
      -> cppwiki::storage::SaveWorkspaceRootResult override {
    materialized_workspaces.insert(record.workspace_id);
    return {};
  }

  [[nodiscard]] auto LoadWorkspaceRoot(std::string_view workspace_id)
      -> std::optional<cppwiki::storage::WorkspaceRootRecord> override {
    if (sync_status.initial_pull_completed) {
      materialized_workspaces.insert(std::string(workspace_id));
    }
    if (materialized_workspaces.contains(std::string(workspace_id))) {
      return cppwiki::storage::WorkspaceRootRecord{
          .workspace_id = std::string(workspace_id),
          .title = std::string(workspace_id),
          .created_at = "2026-07-01T12:00:00Z",
          .schema_version = 1,
      };
    }
    return std::nullopt;
  }

  [[nodiscard]] auto ListWorkspaces() -> cppwiki::storage::ListWorkspacesResult override {
    return cppwiki::storage::ListWorkspacesResult{
        .workspace_ids = {materialized_workspaces.begin(), materialized_workspaces.end()},
        .error = std::nullopt,
    };
  }

  bool bootstrap_applied = false;
  bool start_sync_called = false;
  bool stop_sync_called = false;
  bool access_token_set = false;
  cppwiki::sync::SyncBootstrap last_bootstrap;
  std::string last_access_token;
  cppwiki::storage::SyncStatus sync_status{};
  std::set<std::string> materialized_workspaces;
  std::vector<cppwiki::storage::DocumentSummary> listed_documents;
};

auto MakeSettings(const QString& app_data_directory = MakeUniqueAppDataDirectory())
    -> cppwiki::ProgramSettings {
  return cppwiki::ProgramSettings(
      cppwiki::ToQString(cppwiki::constants::kApplicationName),
      cppwiki::ToQString(cppwiki::constants::kApplicationVersion),
      cppwiki::ToQString(cppwiki::constants::kOrganizationName), app_data_directory,
      QStringLiteral("/tmp/cppwiki-db"), QStringLiteral("/tmp/cppwiki-editor"),
      QStringLiteral("http://127.0.0.1:8080"), true,
      QStringLiteral("http://127.0.0.1:9000/application/o/authorize/"),
      QStringLiteral("http://127.0.0.1:9000/application/o/token/"),
      QStringLiteral("cppwiki-desktop"), QStringLiteral("http://127.0.0.1:38080/auth/callback"),
      true, false, {}, true, 12);
}

auto MakeBaseBootstrap() -> cppwiki::sync::SyncBootstrap {
  cppwiki::sync::SyncBootstrap bootstrap;
  bootstrap.available = true;
  bootstrap.enabled = true;
  bootstrap.gateway_url = QStringLiteral("http://127.0.0.1:4984/cppwiki");
  bootstrap.database_name = QStringLiteral("cppwiki");
  bootstrap.auth_mode = QStringLiteral("oidc_access_token_passthrough");
  bootstrap.token_passthrough = true;
  bootstrap.principal_subject = QStringLiteral("subject-1");
  bootstrap.principal_username = QStringLiteral("akadmin");
  bootstrap.principal_email = QStringLiteral("mail@example.com");
  bootstrap.channels = {QStringLiteral("user:subject-1")};
  bootstrap.status_text = QStringLiteral("Sync bootstrap is ready");
  return bootstrap;
}

auto TestMissingChannelsIsRejected() -> void {
  cppwiki::sync::DocumentSyncService service;
  service.ApplySettings(MakeSettings());
  auto repository = std::make_shared<FakeDocumentRepository>();
  service.SetRepository(repository);
  service.SetAccessToken(QStringLiteral("test-token"));

  auto bootstrap = MakeBaseBootstrap();
  bootstrap.channels.clear();
  service.SetBackendBootstrap(std::move(bootstrap));

  Require(service.State() == cppwiki::sync::DocumentSyncState::kUnavailable,
          "sync service should reject bootstrap without channels");
  if (!service.StatusText().contains(QStringLiteral("did not assign sync channels"))) {
    spdlog::error("Observed status text: {}", service.StatusText().toStdString());
  }
  Require(service.StatusText().contains(QStringLiteral("did not assign sync channels")),
          "sync service should explain missing channels");
  Require(!repository->bootstrap_applied,
          "sync service should not configure repository when bootstrap is invalid");
}

auto TestMissingTokenPassthroughIsRejected() -> void {
  cppwiki::sync::DocumentSyncService service;
  service.ApplySettings(MakeSettings());
  auto repository = std::make_shared<FakeDocumentRepository>();
  service.SetRepository(repository);
  service.SetAccessToken(QStringLiteral("test-token"));

  auto bootstrap = MakeBaseBootstrap();
  bootstrap.token_passthrough = false;
  service.SetBackendBootstrap(std::move(bootstrap));

  Require(service.State() == cppwiki::sync::DocumentSyncState::kUnavailable,
          "sync service should reject bootstrap without token passthrough");
  Require(service.StatusText().contains(QStringLiteral("does not allow token passthrough")),
          "sync service should explain missing token passthrough");
  Require(!repository->bootstrap_applied,
          "sync service should not configure repository without token passthrough");
}

auto TestValidBootstrapConfiguresRepositoryLifecycle() -> void {
  cppwiki::sync::DocumentSyncService service;
  service.ApplySettings(MakeSettings());
  auto repository = std::make_shared<FakeDocumentRepository>();
  service.SetRepository(repository);
  service.SetAccessToken(QStringLiteral("test-token"));
  service.SetBackendBootstrap(MakeBaseBootstrap());

  Require(service.State() == cppwiki::sync::DocumentSyncState::kReady,
          "sync service should become ready with valid bootstrap");
  Require(repository->bootstrap_applied,
          "sync service should apply bootstrap to sync-capable repository");
  Require(repository->access_token_set,
          "sync service should pass access token to sync-capable repository");
  Require(repository->last_access_token == "test-token",
          "repository should receive current access token");
  Require(repository->start_sync_called, "sync service should start repository sync lifecycle");
  Require(repository->last_bootstrap.gateway_url == QStringLiteral("http://127.0.0.1:4984/cppwiki"),
          "repository should receive backend bootstrap");
  const auto snapshot = service.Snapshot();
  Require(snapshot.workspace_ids.contains(QStringLiteral("default")) == false,
          "workspace ids should be empty when no workspace channels are assigned");
}

auto TestSnapshotExposesWorkspaceScopeAndConflicts() -> void {
  cppwiki::sync::DocumentSyncService service;
  service.ApplySettings(MakeSettings());
  auto repository = std::make_shared<FakeDocumentRepository>();
  repository->sync_status = {
      .state = cppwiki::storage::SyncLifecycleState::kRunning,
      .status_text = "Sync running with conflicts",
      .initial_pull_active = true,
      .initial_pull_completed = false,
      .has_conflicts = true,
      .conflict_count = 2,
  };
  service.SetRepository(repository);
  service.SetAccessToken(QStringLiteral("test-token"));

  auto bootstrap = MakeBaseBootstrap();
  bootstrap.channels = {QStringLiteral("user:subject-1"), QStringLiteral("workspace:default"),
                        QStringLiteral("workspace:engineering")};
  service.SetBackendBootstrap(std::move(bootstrap));

  const auto snapshot = service.Snapshot();
  Require(snapshot.workspace_ids.size() == 2,
          "snapshot should expose workspace ids derived from channels");
  Require(snapshot.workspace_ids.contains(QStringLiteral("default")),
          "snapshot should include default workspace id");
  Require(snapshot.workspace_ids.contains(QStringLiteral("engineering")),
          "snapshot should include engineering workspace id");
  Require(snapshot.hydrated_workspace_ids.isEmpty(),
          "snapshot should not mark workspaces hydrated before initial pull completes");
  Require(snapshot.initial_pull_active, "snapshot should expose active initial pull state");
  Require(!snapshot.initial_pull_completed, "snapshot should expose incomplete initial pull state");
  Require(snapshot.has_conflicts, "snapshot should expose repository conflict state");
  Require(snapshot.conflict_count == 2, "snapshot should expose repository conflict count");
}

auto TestRemoteExpectationTracksInitialPullCompletion() -> void {
  cppwiki::sync::DocumentSyncService service;
  service.ApplySettings(MakeSettings());
  auto repository = std::make_shared<FakeDocumentRepository>();
  service.SetRepository(repository);
  service.SetAccessToken(QStringLiteral("test-token"));

  auto bootstrap = MakeBaseBootstrap();
  bootstrap.channels = {QStringLiteral("user:subject-1"), QStringLiteral("workspace:default")};
  service.SetBackendBootstrap(bootstrap);

  Require(service.ShouldExpectRemoteDocuments(QStringLiteral("default")),
          "workspace should expect remote documents while initial pull is pending");

  repository->sync_status = {
      .state = cppwiki::storage::SyncLifecycleState::kRunning,
      .status_text = "Sync idle",
      .initial_pull_active = false,
      .initial_pull_completed = true,
  };
  service.RefreshStatus();

  Require(!service.ShouldExpectRemoteDocuments(QStringLiteral("default")),
          "workspace should stop expecting remote documents after initial pull completes");
  Require(service.Snapshot().initial_pull_completed,
          "snapshot should expose completed initial pull state");
  Require(service.Snapshot().hydrated_workspace_ids.contains(QStringLiteral("default")),
          "snapshot should mark completed workspace as hydrated");
}

auto TestNewWorkspaceBecomesPendingAfterBootstrapChange() -> void {
  cppwiki::sync::DocumentSyncService service;
  service.ApplySettings(MakeSettings());
  auto repository = std::make_shared<FakeDocumentRepository>();
  service.SetRepository(repository);
  service.SetAccessToken(QStringLiteral("test-token"));

  auto bootstrap = MakeBaseBootstrap();
  bootstrap.channels = {QStringLiteral("user:subject-1"), QStringLiteral("workspace:default")};
  service.SetBackendBootstrap(bootstrap);

  repository->sync_status = {
      .state = cppwiki::storage::SyncLifecycleState::kRunning,
      .status_text = "Sync idle",
      .initial_pull_active = false,
      .initial_pull_completed = true,
  };
  service.RefreshStatus();

  Require(!service.ShouldExpectRemoteDocuments(QStringLiteral("default")),
          "default workspace should be hydrated after completed initial pull");

  bootstrap.channels = {QStringLiteral("user:subject-1"), QStringLiteral("workspace:default"),
                        QStringLiteral("workspace:engineering")};
  service.SetBackendBootstrap(bootstrap);

  const auto snapshot = service.Snapshot();
  Require(!snapshot.hydrated_workspace_ids.contains(QStringLiteral("engineering")),
          "new workspace should not be marked hydrated after bootstrap change");
  Require(service.ShouldExpectRemoteDocuments(QStringLiteral("engineering")),
          "new workspace should expect remote documents until next initial pull completes");
}

auto TestOfflineStatusBootstrapDoesNotEraseHydratedWorkspaceScope() -> void {
  cppwiki::sync::DocumentSyncService service;
  service.ApplySettings(MakeSettings());
  auto repository = std::make_shared<FakeDocumentRepository>();
  service.SetRepository(repository);
  service.SetAccessToken(QStringLiteral("test-token"));

  auto bootstrap = MakeBaseBootstrap();
  bootstrap.channels = {QStringLiteral("user:subject-1"), QStringLiteral("workspace:default"),
                        QStringLiteral("workspace:engineering")};
  bootstrap.principal_subject = QStringLiteral("subject-42");
  service.SetBackendBootstrap(bootstrap);

  repository->sync_status = {
      .state = cppwiki::storage::SyncLifecycleState::kRunning,
      .status_text = "Sync idle",
      .initial_pull_active = false,
      .initial_pull_completed = true,
  };
  service.RefreshStatus();

  cppwiki::sync::SyncBootstrap offline_bootstrap;
  offline_bootstrap.status_text = QStringLiteral("Sync: waiting for backend");
  service.SetBackendBootstrap(offline_bootstrap);

  const auto snapshot = service.Snapshot();
  Require(snapshot.workspace_ids.contains(QStringLiteral("default")),
          "offline status bootstrap should preserve previously known default workspace");
  Require(snapshot.workspace_ids.contains(QStringLiteral("engineering")),
          "offline status bootstrap should preserve previously known engineering workspace");
  Require(snapshot.hydrated_workspace_ids.contains(QStringLiteral("default")),
          "offline status bootstrap should preserve hydrated workspace ids");
  Require(snapshot.hydrated_workspace_ids.contains(QStringLiteral("engineering")),
          "offline status bootstrap should preserve hydrated workspace ids for all workspaces");
  Require(snapshot.bootstrap.principal_subject == QStringLiteral("subject-42"),
          "offline status bootstrap should preserve last valid author context");
}

auto TestStatusOnlyBootstrapExposesBackendReason() -> void {
  cppwiki::sync::DocumentSyncService service;
  service.ApplySettings(MakeSettings());
  auto repository = std::make_shared<FakeDocumentRepository>();
  service.SetRepository(repository);
  service.SetAccessToken(QStringLiteral("test-token"));

  cppwiki::sync::SyncBootstrap status_bootstrap;
  status_bootstrap.status_text =
      QStringLiteral("Sync: authenticated session is not accepted by backend");
  service.SetBackendBootstrap(status_bootstrap);

  Require(service.State() == cppwiki::sync::DocumentSyncState::kUnavailable,
          "status-only bootstrap should keep sync unavailable");
  Require(service.StatusText() == status_bootstrap.status_text,
          "sync service should expose backend bootstrap failure reason");
}

auto TestExpandWorkspaceHydrationStateTracksProgressAndFailure() -> void {
  cppwiki::sync::DocumentSyncService service;
  service.ApplySettings(MakeSettings());
  auto repository = std::make_shared<FakeDocumentRepository>();
  service.SetRepository(repository);
  service.SetAccessToken(QStringLiteral("test-token"));

  auto bootstrap = MakeBaseBootstrap();
  bootstrap.channels = {QStringLiteral("user:subject-1"), QStringLiteral("workspace:default"),
                        QStringLiteral("workspace:engineering")};
  service.SetBackendBootstrap(bootstrap);

  // Initial state after StartSync is running but pull is active.
  const auto pending_snapshot = service.Snapshot();
  Require(pending_snapshot.workspace_hydration.size() == 2,
          "snapshot should expose hydration state for all workspaces");
  Require(pending_snapshot.workspace_hydration.value(QStringLiteral("default")) ==
              cppwiki::sync::WorkspaceHydrationState::kInProgress,
          "default workspace should be in progress while initial pull is active");
  Require(pending_snapshot.workspace_hydration.value(QStringLiteral("engineering")) ==
              cppwiki::sync::WorkspaceHydrationState::kInProgress,
          "engineering workspace should be in progress while initial pull is active");
  Require(service.ShouldExpectRemoteDocuments(QStringLiteral("default")),
          "workspace should expect remote documents while in progress");

  // Complete initial pull.
  repository->sync_status = {
      .state = cppwiki::storage::SyncLifecycleState::kRunning,
      .status_text = "Sync idle",
      .initial_pull_active = false,
      .initial_pull_completed = true,
  };
  service.RefreshStatus();

  const auto materialized_snapshot = service.Snapshot();
  Require(materialized_snapshot.workspace_hydration.value(QStringLiteral("default")) ==
              cppwiki::sync::WorkspaceHydrationState::kMaterialized,
          "default workspace should be materialized after initial pull completes");
  Require(materialized_snapshot.workspace_hydration.value(QStringLiteral("engineering")) ==
              cppwiki::sync::WorkspaceHydrationState::kMaterialized,
          "engineering workspace should be materialized after initial pull completes");
  Require(!service.ShouldExpectRemoteDocuments(QStringLiteral("default")),
          "workspace should not expect remote documents after materialization");

  // Reintroduce a new workspace and simulate failure before completion.
  bootstrap.channels = {QStringLiteral("user:subject-1"), QStringLiteral("workspace:default"),
                        QStringLiteral("workspace:engineering"),
                        QStringLiteral("workspace:product")};
  service.SetBackendBootstrap(bootstrap);

  const auto expanded_snapshot = service.Snapshot();
  Require(expanded_snapshot.workspace_hydration.value(QStringLiteral("product")) ==
              cppwiki::sync::WorkspaceHydrationState::kInProgress,
          "new workspace should start as in progress");
  Require(service.ShouldExpectRemoteDocuments(QStringLiteral("product")),
          "new workspace should expect remote documents while in progress");

  // Simulate a replicator error.
  repository->sync_status = {
      .state = cppwiki::storage::SyncLifecycleState::kError,
      .status_text = "Sync failed",
      .initial_pull_active = false,
      .initial_pull_completed = false,
  };
  service.RefreshStatus();

  const auto failed_snapshot = service.Snapshot();
  Require(failed_snapshot.workspace_hydration.value(QStringLiteral("product")) ==
              cppwiki::sync::WorkspaceHydrationState::kFailed,
          "un-materialized workspace should become failed when replicator errors");
  Require(failed_snapshot.workspace_hydration.value(QStringLiteral("default")) ==
              cppwiki::sync::WorkspaceHydrationState::kMaterialized,
          "materialized workspace should stay materialized when replicator errors");
  Require(!service.ShouldExpectRemoteDocuments(QStringLiteral("product")),
          "failed workspace should not expect remote documents");

  // Recovery path: replicator is running again but still no completion.
  repository->sync_status = {
      .state = cppwiki::storage::SyncLifecycleState::kRunning,
      .status_text = "Sync active",
      .initial_pull_active = true,
      .initial_pull_completed = false,
  };
  service.RefreshStatus();

  const auto recovered_snapshot = service.Snapshot();
  // Failure is sticky: product stays failed until it completes.
  Require(recovered_snapshot.workspace_hydration.value(QStringLiteral("product")) ==
              cppwiki::sync::WorkspaceHydrationState::kFailed,
          "failed workspace should stay failed until a successful pull completes");

  // Completion clears the failed state.
  repository->sync_status = {
      .state = cppwiki::storage::SyncLifecycleState::kRunning,
      .status_text = "Sync idle",
      .initial_pull_active = false,
      .initial_pull_completed = true,
  };
  service.RefreshStatus();

  const auto completed_snapshot = service.Snapshot();
  Require(completed_snapshot.workspace_hydration.value(QStringLiteral("product")) ==
              cppwiki::sync::WorkspaceHydrationState::kMaterialized,
          "failed workspace should become materialized after successful pull");
}

auto TestPersistedSyncContextRestoresWorkspaceScopeAfterRestart() -> void {
  const auto app_data_directory =
      std::filesystem::temp_directory_path() / "cppwiki-sync-context-test";
  std::filesystem::remove_all(app_data_directory);

  {
    cppwiki::sync::DocumentSyncService service;
    service.ApplySettings(MakeSettings(QString::fromStdString(app_data_directory.string())));
    auto repository = std::make_shared<FakeDocumentRepository>();
    service.SetRepository(repository);
    service.SetAccessToken(QStringLiteral("test-token"));

    auto bootstrap = MakeBaseBootstrap();
    bootstrap.principal_subject = QStringLiteral("subject-restart");
    bootstrap.channels = {QStringLiteral("user:subject-1"), QStringLiteral("workspace:default"),
                          QStringLiteral("workspace:engineering")};
    service.SetBackendBootstrap(bootstrap);

    repository->sync_status = {
        .state = cppwiki::storage::SyncLifecycleState::kRunning,
        .status_text = "Sync idle",
        .initial_pull_active = false,
        .initial_pull_completed = true,
    };
    service.RefreshStatus();
  }

  {
    cppwiki::sync::DocumentSyncService service;
    service.ApplySettings(MakeSettings(QString::fromStdString(app_data_directory.string())));

    const auto snapshot = service.Snapshot();
    Require(snapshot.workspace_ids.contains(QStringLiteral("default")),
            "persisted sync context should restore default workspace after restart");
    Require(snapshot.workspace_ids.contains(QStringLiteral("engineering")),
            "persisted sync context should restore engineering workspace after restart");
    Require(snapshot.hydrated_workspace_ids.contains(QStringLiteral("default")),
            "persisted sync context should restore hydrated default workspace after restart");
    Require(snapshot.hydrated_workspace_ids.contains(QStringLiteral("engineering")),
            "persisted sync context should restore hydrated engineering workspace after restart");
    Require(snapshot.workspace_hydration.value(QStringLiteral("default")) ==
                cppwiki::sync::WorkspaceHydrationState::kMaterialized,
            "persisted sync context should restore materialized default hydration state");
    Require(snapshot.workspace_hydration.value(QStringLiteral("engineering")) ==
                cppwiki::sync::WorkspaceHydrationState::kMaterialized,
            "persisted sync context should restore materialized engineering hydration state");
    Require(snapshot.bootstrap.principal_subject == QStringLiteral("subject-restart"),
            "persisted sync context should restore principal subject after restart");
  }

  std::filesystem::remove_all(app_data_directory);
}

auto TestReconnectExpandsWorkspaceScopeAndRehydratesNewWorkspace() -> void {
  cppwiki::sync::DocumentSyncService service;
  service.ApplySettings(MakeSettings());
  auto repository = std::make_shared<FakeDocumentRepository>();
  service.SetRepository(repository);
  service.SetAccessToken(QStringLiteral("test-token"));

  auto bootstrap = MakeBaseBootstrap();
  bootstrap.channels = {QStringLiteral("user:subject-1"), QStringLiteral("workspace:default")};
  service.SetBackendBootstrap(bootstrap);

  repository->sync_status = {
      .state = cppwiki::storage::SyncLifecycleState::kRunning,
      .status_text = "Sync idle",
      .initial_pull_active = false,
      .initial_pull_completed = true,
  };
  service.RefreshStatus();

  Require(service.Snapshot().hydrated_workspace_ids.contains(QStringLiteral("default")),
          "default workspace should be hydrated after first completed pull");

  cppwiki::sync::SyncBootstrap offline_bootstrap;
  offline_bootstrap.status_text = QStringLiteral("Sync: waiting for backend");
  service.SetBackendBootstrap(offline_bootstrap);

  Require(service.Snapshot().hydrated_workspace_ids.contains(QStringLiteral("default")),
          "offline transition should preserve hydrated default workspace");
  Require(!service.ShouldExpectRemoteDocuments(QStringLiteral("default")),
          "offline transition should not make hydrated default workspace pending again");

  bootstrap.channels = {QStringLiteral("user:subject-1"), QStringLiteral("workspace:default"),
                        QStringLiteral("workspace:engineering")};
  service.SetBackendBootstrap(bootstrap);

  Require(service.Snapshot().workspace_ids.contains(QStringLiteral("engineering")),
          "reconnect should expose newly assigned engineering workspace");
  Require(!service.Snapshot().hydrated_workspace_ids.contains(QStringLiteral("engineering")),
          "new workspace should remain non-hydrated until next pull completes");
  Require(service.ShouldExpectRemoteDocuments(QStringLiteral("engineering")),
          "new workspace should expect remote documents after reconnect");

  repository->sync_status = {
      .state = cppwiki::storage::SyncLifecycleState::kRunning,
      .status_text = "Sync active",
      .initial_pull_active = true,
      .initial_pull_completed = false,
  };
  service.RefreshStatus();

  Require(service.Snapshot().initial_pull_active,
          "second pull should expose active initial pull state while reconnect hydration runs");

  repository->sync_status = {
      .state = cppwiki::storage::SyncLifecycleState::kRunning,
      .status_text = "Sync idle",
      .initial_pull_active = false,
      .initial_pull_completed = true,
  };
  service.RefreshStatus();

  Require(service.Snapshot().hydrated_workspace_ids.contains(QStringLiteral("engineering")),
          "new workspace should become hydrated after reconnect pull completes");
  Require(!service.ShouldExpectRemoteDocuments(QStringLiteral("engineering")),
          "new workspace should stop expecting remote documents after reconnect hydration");
}

auto TestOfflineReconnectDoesNotRevertHydratedWorkspacesToPending() -> void {
  cppwiki::sync::DocumentSyncService service;
  service.ApplySettings(MakeSettings());
  auto repository = std::make_shared<FakeDocumentRepository>();
  service.SetRepository(repository);
  service.SetAccessToken(QStringLiteral("test-token"));

  auto bootstrap = MakeBaseBootstrap();
  bootstrap.channels = {QStringLiteral("user:subject-1"), QStringLiteral("workspace:default"),
                        QStringLiteral("workspace:engineering")};
  service.SetBackendBootstrap(bootstrap);

  repository->sync_status = {
      .state = cppwiki::storage::SyncLifecycleState::kRunning,
      .status_text = "Sync idle",
      .initial_pull_active = false,
      .initial_pull_completed = true,
  };
  service.RefreshStatus();

  Require(service.Snapshot().hydrated_workspace_ids.contains(QStringLiteral("default")),
          "default workspace should be hydrated after initial pull");
  Require(service.Snapshot().hydrated_workspace_ids.contains(QStringLiteral("engineering")),
          "engineering workspace should be hydrated after initial pull");
  Require(!service.ShouldExpectRemoteDocuments(QStringLiteral("default")),
          "hydrated default workspace should not expect remote documents");
  Require(!service.ShouldExpectRemoteDocuments(QStringLiteral("engineering")),
          "hydrated engineering workspace should not expect remote documents");

  cppwiki::sync::SyncBootstrap offline_bootstrap;
  offline_bootstrap.status_text = QStringLiteral("Sync: waiting for backend");
  service.SetBackendBootstrap(offline_bootstrap);

  Require(service.Snapshot().initial_pull_completed,
          "offline bootstrap should preserve completed initial pull state for hydrated workspaces");
  Require(!service.ShouldExpectRemoteDocuments(QStringLiteral("default")),
          "offline transition should keep hydrated default workspace non-pending");
  Require(!service.ShouldExpectRemoteDocuments(QStringLiteral("engineering")),
          "offline transition should keep hydrated engineering workspace non-pending");

  service.SetBackendBootstrap(bootstrap);

  Require(!service.ShouldExpectRemoteDocuments(QStringLiteral("default")),
          "reconnect with unchanged workspace scope should not make default pending again");
  Require(!service.ShouldExpectRemoteDocuments(QStringLiteral("engineering")),
          "reconnect with unchanged workspace scope should not make engineering pending again");
}

auto TestWorkspaceWithLocalDocumentsIsMaterializedWithoutRootRecord() -> void {
  cppwiki::sync::DocumentSyncService service;
  service.ApplySettings(MakeSettings());
  auto repository = std::make_shared<FakeDocumentRepository>();
  repository->listed_documents = {
      cppwiki::storage::DocumentSummary{
          .id = "page-1",
          .title = "Doc",
          .workspace_id = "engineering",
          .parent_id = std::nullopt,
          .sort_order = 0,
          .created_at = "2026-07-01T12:00:00Z",
          .updated_at = "2026-07-01T12:00:00Z",
          .created_by = "alice",
          .updated_by = "alice",
          .content_version = 1,
      },
  };
  service.SetRepository(repository);
  service.SetAccessToken(QStringLiteral("test-token"));

  auto bootstrap = MakeBaseBootstrap();
  bootstrap.channels = {QStringLiteral("user:subject-1"), QStringLiteral("workspace:engineering")};
  service.SetBackendBootstrap(bootstrap);

  repository->sync_status = {
      .state = cppwiki::storage::SyncLifecycleState::kRunning,
      .status_text = "Sync idle",
      .initial_pull_active = false,
      .initial_pull_completed = false,
  };
  service.RefreshStatus();

  const auto snapshot = service.Snapshot();
  Require(snapshot.workspace_hydration.value(QStringLiteral("engineering")) ==
              cppwiki::sync::WorkspaceHydrationState::kMaterialized,
          "workspace with local documents should be materialized even without root record");
  Require(snapshot.hydrated_workspace_ids.contains(QStringLiteral("engineering")),
          "workspace with local documents should be added to hydrated workspace ids");
  Require(!service.ShouldExpectRemoteDocuments(QStringLiteral("engineering")),
          "workspace with local documents should not be treated as not downloaded");
}

#ifdef CPPWIKI_ENABLE_CBLITE_STORAGE
auto TestReadyStateIncludesPrincipalAndChannels() -> void {
  cppwiki::sync::DocumentSyncService service;
  service.ApplySettings(MakeSettings());
  auto repository = std::make_shared<FakeDocumentRepository>();
  service.SetRepository(repository);
  service.SetAccessToken(QStringLiteral("test-token"));

  auto bootstrap = MakeBaseBootstrap();
  bootstrap.channels = {QStringLiteral("user:subject-1"), QStringLiteral("workspace:default")};
  service.SetBackendBootstrap(std::move(bootstrap));

  Require(service.State() == cppwiki::sync::DocumentSyncState::kReady,
          "sync service should become ready with valid bootstrap and cblite repository");
  Require(service.StatusText().contains(QStringLiteral("akadmin")),
          "ready text should include principal username");
  Require(service.StatusText().contains(QStringLiteral("2 channels")),
          "ready text should include assigned channel count");
}
#endif

}  // namespace

auto main(int argc, char* argv[]) -> int {
  QCoreApplication application(argc, argv);
  QCoreApplication::setApplicationName(cppwiki::ToQString(cppwiki::constants::kApplicationName));
  QCoreApplication::setApplicationVersion(
      cppwiki::ToQString(cppwiki::constants::kApplicationVersion));
  QCoreApplication::setOrganizationName(cppwiki::ToQString(cppwiki::constants::kOrganizationName));

  TestMissingChannelsIsRejected();
  TestMissingTokenPassthroughIsRejected();
  TestValidBootstrapConfiguresRepositoryLifecycle();
  TestSnapshotExposesWorkspaceScopeAndConflicts();
  TestRemoteExpectationTracksInitialPullCompletion();
  TestNewWorkspaceBecomesPendingAfterBootstrapChange();
  TestOfflineStatusBootstrapDoesNotEraseHydratedWorkspaceScope();
  TestStatusOnlyBootstrapExposesBackendReason();
  TestExpandWorkspaceHydrationStateTracksProgressAndFailure();
  TestPersistedSyncContextRestoresWorkspaceScopeAfterRestart();
  TestReconnectExpandsWorkspaceScopeAndRehydratesNewWorkspace();
  TestOfflineReconnectDoesNotRevertHydratedWorkspacesToPending();
  TestWorkspaceWithLocalDocumentsIsMaterializedWithoutRootRecord();
#ifdef CPPWIKI_ENABLE_CBLITE_STORAGE
  TestReadyStateIncludesPrincipalAndChannels();
#endif

  spdlog::info("cppwiki_document_sync_service_tests passed");
  return EXIT_SUCCESS;
}
