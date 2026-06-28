#ifndef CPPWIKI_SRC_SYNC_DOCUMENT_SYNC_SERVICE_H_
#define CPPWIKI_SRC_SYNC_DOCUMENT_SYNC_SERVICE_H_

#include <QObject>
#include <QString>

#include <memory>

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

class DocumentSyncService final : public QObject {
  Q_OBJECT

 public:
  explicit DocumentSyncService(QObject* parent = nullptr);

  void ApplySettings(const ProgramSettings& settings);
  void SetRepository(std::shared_ptr<storage::LocalDocumentRepository> repository);
  void SetAccessToken(QString access_token);
  void SetBackendBootstrap(sync::SyncBootstrap bootstrap);

  [[nodiscard]] auto State() const -> DocumentSyncState;
  [[nodiscard]] auto StatusText() const -> const QString&;
  [[nodiscard]] auto Bootstrap() const -> const sync::SyncBootstrap&;

 signals:
  void statusChanged(cppwiki::sync::DocumentSyncState state, const QString& status_text);

 private:
  void RecomputeStatus();
  void SetStatus(DocumentSyncState state, QString status_text);

  std::shared_ptr<storage::LocalDocumentRepository> repository_;
  QString access_token_;
  sync::SyncBootstrap bootstrap_;
  QString status_text_ = QStringLiteral("Sync: disabled");
  DocumentSyncState state_ = DocumentSyncState::kDisabled;
  bool auth_enabled_ = false;
  bool sync_enabled_ = false;
  bool backend_bootstrap_available_ = false;
  bool backend_sync_enabled_ = false;
};

}  // namespace cppwiki::sync

#endif  // CPPWIKI_SRC_SYNC_DOCUMENT_SYNC_SERVICE_H_
