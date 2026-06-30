#ifndef CPPWIKI_SRC_SYNC_DOCUMENT_SYNC_SERVICE_H_
#define CPPWIKI_SRC_SYNC_DOCUMENT_SYNC_SERVICE_H_

#include <memory>

#include "sync/sync_service.h"

namespace cppwiki {
class ProgramSettings;
}

namespace cppwiki::storage {
class LocalDocumentRepository;
}

namespace cppwiki::sync {

class DocumentSyncService final : public SyncService {
  Q_OBJECT

 public:
  explicit DocumentSyncService(QObject* parent = nullptr);

  void ApplySettings(const ProgramSettings& settings) override;
  void SetRepository(std::shared_ptr<storage::LocalDocumentRepository> repository) override;
  void SetAccessToken(QString access_token) override;
  void SetBackendBootstrap(sync::SyncBootstrap bootstrap) override;

  [[nodiscard]] auto State() const -> DocumentSyncState override;
  [[nodiscard]] auto StatusText() const -> const QString& override;
  [[nodiscard]] auto Bootstrap() const -> const sync::SyncBootstrap& override;
  [[nodiscard]] auto Snapshot() const -> const DocumentSyncSnapshot& override;

 private:
  void RecomputeStatus();
  void SetStatus(DocumentSyncState state, QString status_text);
  void ApplyRepositorySyncLifecycle();
  void RefreshSnapshot();

  std::shared_ptr<storage::LocalDocumentRepository> repository_;
  QString access_token_;
  sync::SyncBootstrap bootstrap_;
  DocumentSyncSnapshot snapshot_;
  bool auth_enabled_ = false;
  bool sync_enabled_ = false;
  bool backend_bootstrap_available_ = false;
  bool backend_sync_enabled_ = false;
};

}  // namespace cppwiki::sync

#endif  // CPPWIKI_SRC_SYNC_DOCUMENT_SYNC_SERVICE_H_
