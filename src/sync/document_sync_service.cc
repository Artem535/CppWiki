#include "sync/document_sync_service.h"

#include "app/program_settings.h"
#include "storage/local_document_repository.h"

namespace cppwiki::sync {

DocumentSyncService::DocumentSyncService(QObject* parent) : SyncService(parent) {
  snapshot_.repository_status.state = storage::SyncLifecycleState::kDisabled;
  snapshot_.repository_status.status_text = "Sync unsupported";
}

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
  return snapshot_.state;
}

auto DocumentSyncService::StatusText() const -> const QString& {
  return snapshot_.status_text;
}

auto DocumentSyncService::Bootstrap() const -> const sync::SyncBootstrap& {
  return bootstrap_;
}

auto DocumentSyncService::Snapshot() const -> const DocumentSyncSnapshot& {
  return snapshot_;
}

void DocumentSyncService::RecomputeStatus() {
  if (repository_ != nullptr &&
      (!auth_enabled_ || !sync_enabled_ || !backend_bootstrap_available_ || !backend_sync_enabled_)) {
    [[maybe_unused]] const auto stop_result = repository_->StopSync();
  }

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

  if (!repository_->SupportsSync()) {
    SetStatus(DocumentSyncState::kUnavailable,
              QStringLiteral("Sync: active repository does not support replication"));
    return;
  }

  if (access_token_.trimmed().isEmpty()) {
    SetStatus(DocumentSyncState::kUnavailable,
              QStringLiteral("Sync: waiting for authenticated session"));
    return;
  }

  ApplyRepositorySyncLifecycle();
  if (const auto sync_status = repository_->GetSyncStatus();
      sync_status.state == storage::SyncLifecycleState::kError) {
    SetStatus(DocumentSyncState::kError, QString::fromStdString(sync_status.status_text));
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
  const auto previous_snapshot = snapshot_;
  const bool status_changed = snapshot_.state != state || snapshot_.status_text != status_text;
  snapshot_.state = state;
  snapshot_.status_text = std::move(status_text);
  RefreshSnapshot();
  const bool snapshot_changed =
      previous_snapshot.state != snapshot_.state ||
      previous_snapshot.status_text != snapshot_.status_text ||
      previous_snapshot.bootstrap.available != snapshot_.bootstrap.available ||
      previous_snapshot.bootstrap.enabled != snapshot_.bootstrap.enabled ||
      previous_snapshot.bootstrap.gateway_url != snapshot_.bootstrap.gateway_url ||
      previous_snapshot.bootstrap.database_name != snapshot_.bootstrap.database_name ||
      previous_snapshot.bootstrap.auth_mode != snapshot_.bootstrap.auth_mode ||
      previous_snapshot.bootstrap.token_passthrough != snapshot_.bootstrap.token_passthrough ||
      previous_snapshot.bootstrap.channels != snapshot_.bootstrap.channels ||
      previous_snapshot.repository_status.state != snapshot_.repository_status.state ||
      previous_snapshot.repository_status.status_text != snapshot_.repository_status.status_text ||
      previous_snapshot.has_repository != snapshot_.has_repository ||
      previous_snapshot.repository_supports_sync != snapshot_.repository_supports_sync ||
      previous_snapshot.has_access_token != snapshot_.has_access_token ||
      previous_snapshot.auth_enabled != snapshot_.auth_enabled ||
      previous_snapshot.sync_enabled != snapshot_.sync_enabled ||
      previous_snapshot.backend_bootstrap_available != snapshot_.backend_bootstrap_available ||
      previous_snapshot.backend_sync_enabled != snapshot_.backend_sync_enabled;

  if (status_changed) {
    emit statusChanged(snapshot_.state, snapshot_.status_text);
  }
  if (snapshot_changed) {
    emit snapshotChanged(snapshot_);
  }
}

void DocumentSyncService::ApplyRepositorySyncLifecycle() {
  if (!repository_) {
    return;
  }

  if (const auto token_result = repository_->SetSyncAccessToken(access_token_.trimmed().toStdString());
      token_result.error) {
    SetStatus(DocumentSyncState::kError,
              QStringLiteral("Sync: %1").arg(QString::fromStdString(token_result.error->message)));
    return;
  }

  if (const auto apply_result = repository_->ApplySyncBootstrap(bootstrap_); apply_result.error) {
    SetStatus(DocumentSyncState::kError,
              QStringLiteral("Sync: %1").arg(QString::fromStdString(apply_result.error->message)));
    return;
  }

  if (const auto start_result = repository_->StartSync(); start_result.error) {
    SetStatus(DocumentSyncState::kError,
              QStringLiteral("Sync: %1").arg(QString::fromStdString(start_result.error->message)));
  }
}

void DocumentSyncService::RefreshSnapshot() {
  snapshot_.bootstrap = bootstrap_;
  snapshot_.auth_enabled = auth_enabled_;
  snapshot_.sync_enabled = sync_enabled_;
  snapshot_.backend_bootstrap_available = backend_bootstrap_available_;
  snapshot_.backend_sync_enabled = backend_sync_enabled_;
  snapshot_.has_repository = repository_ != nullptr;
  snapshot_.repository_supports_sync = repository_ != nullptr && repository_->SupportsSync();
  snapshot_.has_access_token = !access_token_.trimmed().isEmpty();

  if (repository_ != nullptr) {
    snapshot_.repository_status = repository_->GetSyncStatus();
  } else {
    snapshot_.repository_status = storage::SyncStatus{
        .state = storage::SyncLifecycleState::kDisabled,
        .status_text = "Sync repository unavailable",
    };
  }
}

}  // namespace cppwiki::sync
