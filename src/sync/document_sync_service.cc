#include "sync/document_sync_service.h"

#include "app/program_settings.h"
#include "storage/local_document_repository.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace cppwiki::sync {

namespace {

constexpr auto kSyncContextFileName = "sync-context.json";

auto WorkspaceIdsFromChannels(const QStringList& channels) -> QStringList {
  QStringList workspace_ids;
  for (const auto& channel : channels) {
    const auto trimmed = channel.trimmed();
    if (!trimmed.startsWith(QStringLiteral("workspace:"))) {
      continue;
    }

    const auto workspace_id = trimmed.sliced(QStringLiteral("workspace:").size()).trimmed();
    if (!workspace_id.isEmpty() && !workspace_ids.contains(workspace_id)) {
      workspace_ids.push_back(workspace_id);
    }
  }

  return workspace_ids;
}

auto SameSyncSession(const sync::SyncBootstrap& lhs, const sync::SyncBootstrap& rhs) -> bool {
  return lhs.available == rhs.available && lhs.enabled == rhs.enabled &&
         lhs.gateway_url == rhs.gateway_url && lhs.database_name == rhs.database_name &&
         lhs.auth_mode == rhs.auth_mode && lhs.token_passthrough == rhs.token_passthrough &&
         lhs.principal_subject == rhs.principal_subject && lhs.channels == rhs.channels;
}

auto IsUsableSyncBootstrap(const sync::SyncBootstrap& bootstrap) -> bool {
  return bootstrap.available && bootstrap.enabled &&
         !bootstrap.gateway_url.trimmed().isEmpty() &&
         !bootstrap.auth_mode.trimmed().isEmpty() && bootstrap.token_passthrough &&
         !WorkspaceIdsFromChannels(bootstrap.channels).isEmpty();
}

auto IsStatusOnlyBootstrap(const sync::SyncBootstrap& bootstrap) -> bool {
  return !bootstrap.available && !bootstrap.enabled &&
         bootstrap.gateway_url.trimmed().isEmpty() &&
         bootstrap.database_name.trimmed().isEmpty() &&
         bootstrap.auth_mode.trimmed().isEmpty() && !bootstrap.token_passthrough &&
         bootstrap.principal_subject.trimmed().isEmpty() &&
         bootstrap.principal_username.trimmed().isEmpty() &&
         bootstrap.principal_email.trimmed().isEmpty() &&
         bootstrap.principal_roles.isEmpty() && bootstrap.principal_groups.isEmpty() &&
         bootstrap.channels.isEmpty();
}

auto MaterializedWorkspacesFromDocuments(
    const std::shared_ptr<storage::LocalDocumentRepository>& repository) -> QSet<QString> {
  QSet<QString> workspace_ids;
  if (repository == nullptr) {
    return workspace_ids;
  }

  const auto listed = repository->ListDocuments();
  if (listed.error) {
    return workspace_ids;
  }

  for (const auto& document : listed.documents) {
    const auto workspace_id = QString::fromStdString(document.workspace_id).trimmed().isEmpty()
                                  ? QStringLiteral("default")
                                  : QString::fromStdString(document.workspace_id).trimmed();
    workspace_ids.insert(workspace_id);
  }

  return workspace_ids;
}

}  // namespace

DocumentSyncService::DocumentSyncService(QObject* parent) : SyncService(parent) {
  snapshot_.repository_status.state = storage::SyncLifecycleState::kDisabled;
  snapshot_.repository_status.status_text = "Sync unsupported";
  refresh_timer_.setInterval(2000);
  connect(&refresh_timer_, &QTimer::timeout, this, &DocumentSyncService::RefreshStatus);
  refresh_timer_.start();
}

void DocumentSyncService::ApplySettings(const ProgramSettings& settings) {
  const bool first_settings_application = !settings_initialized_;
  const bool auth_changed = auth_enabled_ != settings.AuthEnabled();
  const bool sync_changed = sync_enabled_ != settings.SyncEnabled();
  const bool app_data_dir_changed = app_data_directory_ != settings.AppDataDirectory();
  app_data_directory_ = settings.AppDataDirectory();
  if (app_data_dir_changed) {
    LoadPersistedSyncContext();
  }
  auth_enabled_ = settings.AuthEnabled();
  sync_enabled_ = settings.SyncEnabled();
  if (!first_settings_application && (auth_changed || sync_changed)) {
    ResetWorkspaceHydration();
  }
  settings_initialized_ = true;
  RecomputeStatus();
}

void DocumentSyncService::SetRepository(
    std::shared_ptr<storage::LocalDocumentRepository> repository) {
  if (repository_ != repository) {
    ResetWorkspaceHydration();
  }
  repository_ = std::move(repository);
  RecomputeStatus();
}

void DocumentSyncService::SetAccessToken(QString access_token) {
  if (access_token_ != access_token) {
    ResetWorkspaceHydration();
  }
  access_token_ = std::move(access_token);
  RecomputeStatus();
}

void DocumentSyncService::SetBackendBootstrap(sync::SyncBootstrap bootstrap) {
  backend_bootstrap_available_ = bootstrap.available;
  backend_sync_enabled_ = bootstrap.enabled;

  if (IsStatusOnlyBootstrap(bootstrap)) {
    RecomputeStatus();
    return;
  }

  if (!SameSyncSession(bootstrap_, bootstrap)) {
    ResetWorkspaceHydration();
  }
  bootstrap_ = bootstrap;

  if (IsUsableSyncBootstrap(bootstrap_)) {
    SavePersistedSyncContext();
  }

  RecomputeStatus();
}

void DocumentSyncService::RefreshStatus() {
  const auto previous_snapshot = snapshot_;
  const auto previous_state = snapshot_.state;
  const auto previous_text = snapshot_.status_text;

  RefreshSnapshot();
  if (snapshot_.repository_status.state == storage::SyncLifecycleState::kError) {
    snapshot_.state = DocumentSyncState::kError;
    snapshot_.status_text = QString::fromStdString(snapshot_.repository_status.status_text);
  }

  const bool status_changed =
      previous_state != snapshot_.state || previous_text != snapshot_.status_text;
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
      previous_snapshot.repository_status.has_conflicts !=
          snapshot_.repository_status.has_conflicts ||
      previous_snapshot.repository_status.conflict_count !=
          snapshot_.repository_status.conflict_count ||
      previous_snapshot.has_repository != snapshot_.has_repository ||
      previous_snapshot.repository_supports_sync != snapshot_.repository_supports_sync ||
      previous_snapshot.has_access_token != snapshot_.has_access_token ||
      previous_snapshot.auth_enabled != snapshot_.auth_enabled ||
      previous_snapshot.sync_enabled != snapshot_.sync_enabled ||
      previous_snapshot.backend_bootstrap_available != snapshot_.backend_bootstrap_available ||
      previous_snapshot.backend_sync_enabled != snapshot_.backend_sync_enabled ||
      previous_snapshot.initial_pull_active != snapshot_.initial_pull_active ||
      previous_snapshot.initial_pull_completed != snapshot_.initial_pull_completed ||
      previous_snapshot.has_conflicts != snapshot_.has_conflicts ||
      previous_snapshot.conflict_count != snapshot_.conflict_count ||
      previous_snapshot.workspace_ids != snapshot_.workspace_ids ||
      previous_snapshot.hydrated_workspace_ids != snapshot_.hydrated_workspace_ids ||
      previous_snapshot.workspace_hydration != snapshot_.workspace_hydration;

  if (status_changed) {
    emit statusChanged(snapshot_.state, snapshot_.status_text);
  }
  if (snapshot_changed) {
    emit snapshotChanged(snapshot_);
  }
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
      previous_snapshot.backend_sync_enabled != snapshot_.backend_sync_enabled ||
      previous_snapshot.initial_pull_active != snapshot_.initial_pull_active ||
      previous_snapshot.initial_pull_completed != snapshot_.initial_pull_completed ||
      previous_snapshot.has_conflicts != snapshot_.has_conflicts ||
      previous_snapshot.conflict_count != snapshot_.conflict_count ||
      previous_snapshot.workspace_ids != snapshot_.workspace_ids ||
      previous_snapshot.hydrated_workspace_ids != snapshot_.hydrated_workspace_ids ||
      previous_snapshot.workspace_hydration != snapshot_.workspace_hydration;

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
  snapshot_.workspace_ids = WorkspaceIdsFromChannels(bootstrap_.channels);

  if (repository_ != nullptr) {
    snapshot_.repository_status = repository_->GetSyncStatus();
  } else {
    snapshot_.repository_status = storage::SyncStatus{
        .state = storage::SyncLifecycleState::kDisabled,
        .status_text = "Sync repository unavailable",
    };
  }
  UpdateWorkspaceHydration();
  snapshot_.initial_pull_active = snapshot_.repository_status.initial_pull_active;
  snapshot_.hydrated_workspace_ids = hydrated_workspace_ids_;
  snapshot_.workspace_hydration = workspace_hydration_;
  snapshot_.initial_pull_completed =
      !snapshot_.workspace_ids.isEmpty() &&
      snapshot_.hydrated_workspace_ids.size() == snapshot_.workspace_ids.size();
  snapshot_.has_conflicts = snapshot_.repository_status.has_conflicts;
  snapshot_.conflict_count =
      static_cast<qsizetype>(snapshot_.repository_status.conflict_count);
}

void DocumentSyncService::ResetWorkspaceHydration() {
  hydrated_workspace_ids_.clear();
  workspace_hydration_.clear();
}

void DocumentSyncService::UpdateWorkspaceHydration() {
  // Prune workspaces that are no longer in the bootstrap scope.
  for (auto it = workspace_hydration_.begin(); it != workspace_hydration_.end();) {
    if (!snapshot_.workspace_ids.contains(it.key())) {
      it = workspace_hydration_.erase(it);
    } else {
      ++it;
    }
  }

  const auto repository_state = snapshot_.repository_status.state;
  const auto global_failed = repository_state == storage::SyncLifecycleState::kError;
  bool any_newly_materialized = false;
  const auto workspaces_with_documents = MaterializedWorkspacesFromDocuments(repository_);

  for (const auto& workspace_id : snapshot_.workspace_ids) {
    const auto previous = workspace_hydration_.value(workspace_id,
                                                     WorkspaceHydrationState::kNotStarted);

    // Fact-based hydration: a workspace is materialized if either its explicit
    // root/meta record exists locally or at least one document for that workspace
    // is already present in the local database.
    const bool root_materialized =
        repository_ != nullptr && repository_->LoadWorkspaceRoot(workspace_id.toStdString()).has_value();
    const bool has_local_documents = workspaces_with_documents.contains(workspace_id);

    if (root_materialized || has_local_documents ||
        previous == WorkspaceHydrationState::kMaterialized) {
      if (previous != WorkspaceHydrationState::kMaterialized) {
        any_newly_materialized = true;
      }
      workspace_hydration_[workspace_id] = WorkspaceHydrationState::kMaterialized;
      continue;
    }

    // Sticky failure: once failed, it stays failed until root_materialized is true
    if (previous == WorkspaceHydrationState::kFailed) {
      workspace_hydration_[workspace_id] = WorkspaceHydrationState::kFailed;
      continue;
    }

    // Global failure transitions unmaterialized workspaces to failed
    if (global_failed) {
      workspace_hydration_[workspace_id] = WorkspaceHydrationState::kFailed;
      continue;
    }

    // A workspace stays "in progress" only while the current pull is actually active.
    // `kRunning` also covers the steady-state "Sync idle" replicator status, so it must
    // not by itself keep workspace hydration stuck in progress forever.
    if (snapshot_.repository_status.initial_pull_active) {
      workspace_hydration_[workspace_id] = WorkspaceHydrationState::kInProgress;
      continue;
    }

    workspace_hydration_[workspace_id] = WorkspaceHydrationState::kNotStarted;
  }

  // Back-fill compatibility list for existing UI consumers.
  hydrated_workspace_ids_.clear();
  for (auto it = workspace_hydration_.begin(); it != workspace_hydration_.end(); ++it) {
    if (IsHydrated(it.value())) {
      hydrated_workspace_ids_.push_back(it.key());
    }
  }

  if (any_newly_materialized || snapshot_.repository_status.initial_pull_completed) {
    SavePersistedSyncContext();
  }
}

void DocumentSyncService::LoadPersistedSyncContext() {
  if (app_data_directory_.trimmed().isEmpty()) {
    return;
  }

  const auto file_path = QDir(app_data_directory_).filePath(QString::fromUtf8(kSyncContextFileName));
  QFile file(file_path);
  if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
    return;
  }

  const auto json = QJsonDocument::fromJson(file.readAll());
  if (!json.isObject()) {
    return;
  }

  const auto root = json.object();
  const auto bootstrap = root.value(QStringLiteral("bootstrap")).toObject();
  sync::SyncBootstrap loaded_bootstrap;
  loaded_bootstrap.available = bootstrap.value(QStringLiteral("available")).toBool(false);
  loaded_bootstrap.enabled = bootstrap.value(QStringLiteral("enabled")).toBool(false);
  loaded_bootstrap.gateway_url = bootstrap.value(QStringLiteral("gateway_url")).toString();
  loaded_bootstrap.database_name = bootstrap.value(QStringLiteral("database_name")).toString();
  loaded_bootstrap.auth_mode = bootstrap.value(QStringLiteral("auth_mode")).toString();
  loaded_bootstrap.token_passthrough =
      bootstrap.value(QStringLiteral("token_passthrough")).toBool(false);
  loaded_bootstrap.principal_subject =
      bootstrap.value(QStringLiteral("principal_subject")).toString();
  loaded_bootstrap.principal_username =
      bootstrap.value(QStringLiteral("principal_username")).toString();
  loaded_bootstrap.principal_email = bootstrap.value(QStringLiteral("principal_email")).toString();

  const auto roles = bootstrap.value(QStringLiteral("principal_roles")).toArray();
  for (const auto& role : roles) {
    loaded_bootstrap.principal_roles.push_back(role.toString());
  }
  const auto groups = bootstrap.value(QStringLiteral("principal_groups")).toArray();
  for (const auto& group : groups) {
    loaded_bootstrap.principal_groups.push_back(group.toString());
  }
  const auto channels = bootstrap.value(QStringLiteral("channels")).toArray();
  for (const auto& channel : channels) {
    loaded_bootstrap.channels.push_back(channel.toString());
  }

  if (IsUsableSyncBootstrap(loaded_bootstrap)) {
    bootstrap_ = std::move(loaded_bootstrap);
  }

  hydrated_workspace_ids_.clear();
  workspace_hydration_.clear();

  const auto context_version = root.value(QStringLiteral("version")).toInt(0);

  // Prefer detailed workspace_hydration object when available.
  if (context_version >= 2) {
    const auto hydration = root.value(QStringLiteral("workspace_hydration")).toObject();
    for (auto it = hydration.begin(); it != hydration.end(); ++it) {
      const auto workspace_id = it.key().trimmed();
      if (workspace_id.isEmpty()) {
        continue;
      }
      const auto state_text = it.value().toString();
      if (state_text == QStringLiteral("materialized")) {
        workspace_hydration_[workspace_id] = WorkspaceHydrationState::kMaterialized;
      } else if (state_text == QStringLiteral("failed")) {
        workspace_hydration_[workspace_id] = WorkspaceHydrationState::kFailed;
      } else if (state_text == QStringLiteral("in_progress")) {
        workspace_hydration_[workspace_id] = WorkspaceHydrationState::kInProgress;
      } else if (state_text == QStringLiteral("not_started")) {
        workspace_hydration_[workspace_id] = WorkspaceHydrationState::kNotStarted;
      }
    }
  }

  // Fallback: hydrate from the pre-v2 list.
  const auto hydrated = root.value(QStringLiteral("hydrated_workspace_ids")).toArray();
  for (const auto& workspace_id : hydrated) {
    const auto value = workspace_id.toString().trimmed();
    if (!value.isEmpty() && !hydrated_workspace_ids_.contains(value)) {
      hydrated_workspace_ids_.push_back(value);
      // Lift into the detailed map if it was not already set.
      if (!workspace_hydration_.contains(value)) {
        workspace_hydration_[value] = WorkspaceHydrationState::kMaterialized;
      }
    }
  }
}

void DocumentSyncService::SavePersistedSyncContext() const {
  if (app_data_directory_.trimmed().isEmpty() || !IsUsableSyncBootstrap(bootstrap_)) {
    return;
  }

  QDir directory(app_data_directory_);
  if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
    return;
  }

  QJsonObject bootstrap;
  bootstrap.insert(QStringLiteral("available"), bootstrap_.available);
  bootstrap.insert(QStringLiteral("enabled"), bootstrap_.enabled);
  bootstrap.insert(QStringLiteral("gateway_url"), bootstrap_.gateway_url);
  bootstrap.insert(QStringLiteral("database_name"), bootstrap_.database_name);
  bootstrap.insert(QStringLiteral("auth_mode"), bootstrap_.auth_mode);
  bootstrap.insert(QStringLiteral("token_passthrough"), bootstrap_.token_passthrough);
  bootstrap.insert(QStringLiteral("principal_subject"), bootstrap_.principal_subject);
  bootstrap.insert(QStringLiteral("principal_username"), bootstrap_.principal_username);
  bootstrap.insert(QStringLiteral("principal_email"), bootstrap_.principal_email);

  QJsonArray roles;
  for (const auto& role : bootstrap_.principal_roles) {
    roles.push_back(role);
  }
  bootstrap.insert(QStringLiteral("principal_roles"), roles);

  QJsonArray groups;
  for (const auto& group : bootstrap_.principal_groups) {
    groups.push_back(group);
  }
  bootstrap.insert(QStringLiteral("principal_groups"), groups);

  QJsonArray channels;
  for (const auto& channel : bootstrap_.channels) {
    channels.push_back(channel);
  }
  bootstrap.insert(QStringLiteral("channels"), channels);

  QJsonArray hydrated_workspaces;
  for (const auto& workspace_id : hydrated_workspace_ids_) {
    hydrated_workspaces.push_back(workspace_id);
  }

  QJsonObject hydration;
  for (auto it = workspace_hydration_.begin(); it != workspace_hydration_.end(); ++it) {
    QString state_text;
    switch (it.value()) {
      case WorkspaceHydrationState::kNotStarted:
        state_text = QStringLiteral("not_started");
        break;
      case WorkspaceHydrationState::kInProgress:
        state_text = QStringLiteral("in_progress");
        break;
      case WorkspaceHydrationState::kMaterialized:
        state_text = QStringLiteral("materialized");
        break;
      case WorkspaceHydrationState::kFailed:
        state_text = QStringLiteral("failed");
        break;
    }
    hydration.insert(it.key(), state_text);
  }

  QJsonObject root;
  root.insert(QStringLiteral("version"), 2);
  root.insert(QStringLiteral("bootstrap"), bootstrap);
  root.insert(QStringLiteral("hydrated_workspace_ids"), hydrated_workspaces);
  root.insert(QStringLiteral("workspace_hydration"), hydration);

  QFile file(directory.filePath(QString::fromUtf8(kSyncContextFileName)));
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return;
  }

  file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

}  // namespace cppwiki::sync
