#include "sync/document_sync_service.h"

#include "app/program_settings.h"
#include "storage/local_document_repository.h"

#ifdef CPPWIKI_ENABLE_CBLITE_STORAGE
#include "storage/cblite_document_repository.h"
#endif

namespace cppwiki::sync {

DocumentSyncService::DocumentSyncService(QObject* parent) : QObject(parent) {}

void DocumentSyncService::ApplySettings(const ProgramSettings& settings) {
  auth_enabled_ = settings.AuthEnabled();
  sync_enabled_ = settings.SyncEnabled();
  RecomputeStatus();
}

void DocumentSyncService::SetRepository(
    std::shared_ptr<storage::LocalDocumentRepository> repository) {
  repository_ = std::move(repository);
  RecomputeStatus();
}

void DocumentSyncService::SetAccessToken(QString access_token) {
  access_token_ = std::move(access_token);
  RecomputeStatus();
}

void DocumentSyncService::SetBackendBootstrap(sync::SyncBootstrap bootstrap) {
  backend_bootstrap_available_ = bootstrap.available;
  backend_sync_enabled_ = bootstrap.enabled;
  bootstrap_ = std::move(bootstrap);
  RecomputeStatus();
}

auto DocumentSyncService::State() const -> DocumentSyncState {
  return state_;
}

auto DocumentSyncService::StatusText() const -> const QString& {
  return status_text_;
}

auto DocumentSyncService::Bootstrap() const -> const sync::SyncBootstrap& {
  return bootstrap_;
}

void DocumentSyncService::RecomputeStatus() {
  if (!auth_enabled_) {
    SetStatus(DocumentSyncState::kDisabled, QStringLiteral("Sync: auth disabled"));
    return;
  }

  if (!sync_enabled_) {
    SetStatus(DocumentSyncState::kDisabled, QStringLiteral("Sync: disabled"));
    return;
  }

  if (!backend_bootstrap_available_) {
    SetStatus(DocumentSyncState::kUnavailable,
              bootstrap_.status_text.trimmed().isEmpty()
                  ? QStringLiteral("Sync: waiting for backend bootstrap")
                  : bootstrap_.status_text);
    return;
  }

  if (!backend_sync_enabled_) {
    SetStatus(DocumentSyncState::kUnavailable,
              bootstrap_.status_text.trimmed().isEmpty()
                  ? QStringLiteral("Sync: disabled by backend")
                  : bootstrap_.status_text);
    return;
  }

  if (bootstrap_.gateway_url.trimmed().isEmpty()) {
    SetStatus(DocumentSyncState::kUnavailable,
              QStringLiteral("Sync: backend bootstrap is missing gateway URL"));
    return;
  }

  if (bootstrap_.auth_mode.trimmed().isEmpty()) {
    SetStatus(DocumentSyncState::kUnavailable,
              QStringLiteral("Sync: backend bootstrap is missing auth mode"));
    return;
  }

  if (!bootstrap_.token_passthrough) {
    SetStatus(DocumentSyncState::kUnavailable,
              QStringLiteral("Sync: backend bootstrap does not allow token passthrough"));
    return;
  }

  if (bootstrap_.channels.isEmpty()) {
    SetStatus(DocumentSyncState::kUnavailable,
              QStringLiteral("Sync: backend did not assign sync channels"));
    return;
  }

  if (!repository_) {
    SetStatus(DocumentSyncState::kUnavailable, QStringLiteral("Sync: repository unavailable"));
    return;
  }

#ifndef CPPWIKI_ENABLE_CBLITE_STORAGE
  SetStatus(DocumentSyncState::kUnavailable,
            QStringLiteral("Sync: Couchbase Lite support not built"));
  return;
#else
  if (dynamic_cast<storage::CbliteDocumentRepository*>(repository_.get()) == nullptr) {
    SetStatus(DocumentSyncState::kUnavailable,
              QStringLiteral("Sync: active repository does not support replication"));
    return;
  }
#endif

  if (access_token_.trimmed().isEmpty()) {
    SetStatus(DocumentSyncState::kUnavailable,
              QStringLiteral("Sync: waiting for authenticated session"));
    return;
  }

  SetStatus(DocumentSyncState::kReady,
            QStringLiteral("Sync: ready for %1 at %2 as %3 (%4 channel%5)")
                .arg(bootstrap_.database_name.trimmed().isEmpty() ? QStringLiteral("replication")
                                                                  : bootstrap_.database_name,
                     bootstrap_.gateway_url,
                     bootstrap_.principal_username.trimmed().isEmpty()
                         ? QStringLiteral("current user")
                         : bootstrap_.principal_username,
                     QString::number(bootstrap_.channels.size()),
                     bootstrap_.channels.size() == 1 ? QString{} : QStringLiteral("s")));
}

void DocumentSyncService::SetStatus(DocumentSyncState state, QString status_text) {
  if (state_ == state && status_text_ == status_text) {
    return;
  }

  state_ = state;
  status_text_ = std::move(status_text);
  emit statusChanged(state_, status_text_);
}

}  // namespace cppwiki::sync
