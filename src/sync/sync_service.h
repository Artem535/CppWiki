#ifndef CPPWIKI_SRC_SYNC_SYNC_SERVICE_H_
#define CPPWIKI_SRC_SYNC_SYNC_SERVICE_H_

#include <QObject>
#include <QString>

#include <memory>

#include "storage/local_document_repository.h"
#include "sync/sync_bootstrap.h"

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
};

class SyncService : public QObject {
  Q_OBJECT

 public:
  explicit SyncService(QObject* parent = nullptr) : QObject(parent) {}
  ~SyncService() override = default;

  virtual void ApplySettings(const ProgramSettings& settings) = 0;
  virtual void SetRepository(std::shared_ptr<storage::LocalDocumentRepository> repository) = 0;
  virtual void SetAccessToken(QString access_token) = 0;
  virtual void SetBackendBootstrap(sync::SyncBootstrap bootstrap) = 0;

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
