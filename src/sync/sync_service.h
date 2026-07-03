#ifndef CPPWIKI_SRC_SYNC_SYNC_SERVICE_H_
#define CPPWIKI_SRC_SYNC_SYNC_SERVICE_H_

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

#include "storage/local_document_repository.h"
#include "sync/sync_bootstrap.h"
#include "sync/sync_state_provider.h"

namespace cppwiki {
class ProgramSettings;
}

namespace cppwiki::storage {
class LocalDocumentRepository;
}

namespace cppwiki::sync {

enum class DocumentSyncState {
  kDisabled,
  kUnavailable,
  kReady,
  kError,
};

enum class WorkspaceHydrationState {
  kNotStarted,
  kInProgress,
  kMaterialized,
  kFailed,
};

struct DocumentSyncSnapshot {
  DocumentSyncState state{DocumentSyncState::kDisabled};
  QString status_text{QStringLiteral("Sync: disabled")};
  sync::SyncBootstrap bootstrap;
  storage::SyncStatus repository_status{};
  bool has_repository = false;
  bool repository_supports_sync = false;
  bool has_access_token = false;
  bool auth_enabled = false;
  bool sync_enabled = false;
  bool backend_bootstrap_available = false;
  bool backend_sync_enabled = false;
  bool initial_pull_active = false;
  bool initial_pull_completed = false;
  bool has_conflicts = false;
  qsizetype conflict_count = 0;
  QStringList workspace_ids;
  QStringList hydrated_workspace_ids;
  QHash<QString, WorkspaceHydrationState> workspace_hydration;
};

[[nodiscard]] inline auto IsHydrated(WorkspaceHydrationState state) -> bool {
  return state == WorkspaceHydrationState::kMaterialized;
}

[[nodiscard]] inline auto IsPendingOrInProgress(WorkspaceHydrationState state) -> bool {
  return state == WorkspaceHydrationState::kNotStarted ||
         state == WorkspaceHydrationState::kInProgress;
}

class SyncService : public QObject, public SyncStateProvider {
  Q_OBJECT

 public:
  explicit SyncService(QObject* parent = nullptr) : QObject(parent) {}
  ~SyncService() override = default;

  [[nodiscard]] auto ShouldExpectRemoteDocuments(const QString& workspace_id) const
      -> bool override {
    const auto normalized_workspace_id = workspace_id.trimmed().isEmpty()
                                             ? QStringLiteral("default")
                                             : workspace_id.trimmed();
    const auto& snapshot = Snapshot();
    const bool repository_ready =
        snapshot.sync_enabled && snapshot.auth_enabled && snapshot.has_repository &&
        snapshot.repository_supports_sync;
    const bool has_bootstrap_scope =
        snapshot.bootstrap.token_passthrough &&
        !snapshot.bootstrap.gateway_url.trimmed().isEmpty() &&
        !snapshot.bootstrap.auth_mode.trimmed().isEmpty() &&
        snapshot.workspace_ids.contains(normalized_workspace_id);

    // Cold start after local DB reset must still treat previously synced
    // workspaces as remote-backed even before the live auth/backend session is
    // fully re-established. Otherwise the UI eagerly creates local welcome
    // pages and pollutes an empty database before the initial pull begins.
    if (!repository_ready || !has_bootstrap_scope) {
      return false;
    }

    const auto state = snapshot.workspace_hydration.value(normalized_workspace_id,
                                                          WorkspaceHydrationState::kNotStarted);
    return IsPendingOrInProgress(state);
  }

  [[nodiscard]] auto ShouldCreateSyntheticWelcomePage(const QString& workspace_id) const
      -> bool override {
    Q_UNUSED(workspace_id);

    const auto& snapshot = Snapshot();

    // In authenticated sync mode the local DB must not be eagerly seeded with
    // synthetic welcome pages. An empty DB should stay empty until the initial
    // pull either materializes remote documents or the user explicitly creates
    // local content offline.
    if (snapshot.sync_enabled && snapshot.auth_enabled && snapshot.has_repository &&
        snapshot.repository_supports_sync) {
      return false;
    }

    return true;
  }

  virtual void ApplySettings(const ProgramSettings& settings) = 0;
  virtual void SetRepository(std::shared_ptr<storage::LocalDocumentRepository> repository) = 0;
  virtual void SetAccessToken(QString access_token) = 0;
  virtual void SetBackendBootstrap(sync::SyncBootstrap bootstrap) = 0;
  virtual void RefreshStatus() = 0;

  [[nodiscard]] virtual auto State() const -> DocumentSyncState = 0;
  [[nodiscard]] virtual auto StatusText() const -> const QString& = 0;
  [[nodiscard]] virtual auto Bootstrap() const -> const sync::SyncBootstrap& = 0;
  [[nodiscard]] virtual auto Snapshot() const -> const DocumentSyncSnapshot& = 0;

 signals:
  void statusChanged(cppwiki::sync::DocumentSyncState state, const QString& status_text);
  void snapshotChanged(const cppwiki::sync::DocumentSyncSnapshot& snapshot);
};

}  // namespace cppwiki::sync

Q_DECLARE_METATYPE(cppwiki::sync::DocumentSyncSnapshot)

#endif  // CPPWIKI_SRC_SYNC_SYNC_SERVICE_H_
