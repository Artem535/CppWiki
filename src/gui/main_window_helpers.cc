#include "gui/main_window_helpers.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

#include <oclero/qlementine/widgets/StatusBadgeWidget.hpp>

#include "auth/auth_session_manager.h"

namespace cppwiki::gui::main_window_helpers {

auto StateTextColor(bool is_error, bool is_warning, bool is_success) -> QString {
  if (is_error) {
    return QStringLiteral("#ff7b72");
  }
  if (is_warning) {
    return QStringLiteral("#e3b341");
  }
  if (is_success) {
    return QStringLiteral("#7ee787");
  }
  return QStringLiteral("#d0d7de");
}

auto BoolLabel(bool value, QStringView true_text, QStringView false_text)
    -> QString {
  return value ? true_text.toString() : false_text.toString();
}

auto SyncLifecycleStateLabel(storage::SyncLifecycleState state) -> QString {
  switch (state) {
    case storage::SyncLifecycleState::kDisabled:
      return QStringLiteral("Disabled");
    case storage::SyncLifecycleState::kConfigured:
      return QStringLiteral("Configured");
    case storage::SyncLifecycleState::kRunning:
      return QStringLiteral("Running");
    case storage::SyncLifecycleState::kError:
      return QStringLiteral("Error");
  }

  return QStringLiteral("Unknown");
}

auto JoinOrFallback(const QStringList& values, const QString& fallback)
    -> QString {
  if (values.isEmpty()) {
    return fallback;
  }

  return values.join(QStringLiteral(", "));
}

auto HydrationStateLabel(sync::WorkspaceHydrationState state) -> QString {
  switch (state) {
    case sync::WorkspaceHydrationState::kNotStarted:
      return QStringLiteral("Not started");
    case sync::WorkspaceHydrationState::kInProgress:
      return QStringLiteral("In progress");
    case sync::WorkspaceHydrationState::kMaterialized:
      return QStringLiteral("Materialized");
    case sync::WorkspaceHydrationState::kFailed:
      return QStringLiteral("Failed");
  }
  return QStringLiteral("Unknown");
}

auto BuildWorkspaceHydrationSummary(const sync::DocumentSyncSnapshot& snapshot) -> QString {
  if (snapshot.workspace_ids.isEmpty()) {
    return QStringLiteral("No sync workspaces assigned.");
  }

  QStringList lines;
  for (const auto& workspace_id : snapshot.workspace_ids) {
    const auto state = snapshot.workspace_hydration.value(workspace_id,
                                                          sync::WorkspaceHydrationState::kNotStarted);
    QString state_text = HydrationStateLabel(state);
    switch (state) {
      case sync::WorkspaceHydrationState::kNotStarted:
        state_text = QStringLiteral("%1 — documents not downloaded yet").arg(state_text);
        break;
      case sync::WorkspaceHydrationState::kInProgress:
        state_text = QStringLiteral("%1 — syncing documents...").arg(state_text);
        break;
      case sync::WorkspaceHydrationState::kMaterialized:
        state_text = QStringLiteral("%1 — available offline").arg(state_text);
        break;
      case sync::WorkspaceHydrationState::kFailed:
        state_text = QStringLiteral("%1 — initial pull failed, retry on reconnect").arg(state_text);
        break;
    }
    lines.push_back(QStringLiteral("• %1: %2").arg(workspace_id, state_text));
  }
  return lines.join(QStringLiteral("\n"));
}

auto BuildSyncGuidance(const sync::DocumentSyncSnapshot& snapshot) -> QString {
  if (!snapshot.auth_enabled) {
    return QStringLiteral("Enable authentication in settings before document sync can start.");
  }
  if (!snapshot.sync_enabled) {
    return QStringLiteral("Enable sync in settings for the current desktop client.");
  }
  if (!snapshot.backend_bootstrap_available) {
    return QStringLiteral("Backend has not returned sync bootstrap yet.");
  }
  if (!snapshot.backend_sync_enabled) {
    return QStringLiteral("Backend rejected sync for the current session or workspace.");
  }
  if (!snapshot.has_repository) {
    return QStringLiteral("No local repository is attached to the sync service.");
  }
  if (!snapshot.repository_supports_sync) {
    return QStringLiteral("The active repository implementation does not support replication.");
  }
  if (!snapshot.has_access_token) {
    return QStringLiteral("Authenticated session is missing an access token for Sync Gateway.");
  }
  if (snapshot.bootstrap.gateway_url.trimmed().isEmpty()) {
    return QStringLiteral("Backend bootstrap is missing Sync Gateway URL.");
  }
  if (snapshot.bootstrap.channels.isEmpty()) {
    return QStringLiteral("Backend bootstrap did not assign any sync channels.");
  }
  if (snapshot.repository_status.state == storage::SyncLifecycleState::kError) {
    return QStringLiteral("Repository replication failed after bootstrap was applied.\n\n%1")
        .arg(BuildWorkspaceHydrationSummary(snapshot));
  }
  if (snapshot.initial_pull_active) {
    return QStringLiteral(
               "Initial sync is still downloading remote documents for assigned workspaces.\n\n%1")
        .arg(BuildWorkspaceHydrationSummary(snapshot));
  }
  if (!snapshot.initial_pull_completed) {
    return QStringLiteral("Waiting for initial sync to start.\n\n%1")
        .arg(BuildWorkspaceHydrationSummary(snapshot));
  }
  if (snapshot.has_conflicts) {
    return QStringLiteral("Sync detected %1 pending conflict%2. Resolve or dismiss them from sync details.")
        .arg(QString::number(snapshot.conflict_count),
             snapshot.conflict_count == 1 ? QString{} : QStringLiteral("s"));
  }

  return QStringLiteral("Sync prerequisites are satisfied.\n\n%1")
      .arg(BuildWorkspaceHydrationSummary(snapshot));
}

auto PendingConflicts(const std::shared_ptr<storage::LocalDocumentRepository>& repository)
    -> std::vector<storage::DocumentConflictRecord> {
  if (repository == nullptr) {
    return {};
  }

  auto conflicts = repository->ListConflicts();
  if (conflicts.error) {
    return {};
  }

  std::vector<storage::DocumentConflictRecord> pending_conflicts;
  for (auto& conflict : conflicts.conflicts) {
    if (conflict.resolution_state == "pending") {
      pending_conflicts.push_back(std::move(conflict));
    }
  }
  return pending_conflicts;
}

auto FirstPendingConflict(const std::shared_ptr<storage::LocalDocumentRepository>& repository)
    -> std::optional<storage::DocumentConflictRecord> {
  auto conflicts = PendingConflicts(repository);
  if (conflicts.empty()) {
    return std::nullopt;
  }

  return conflicts.front();
}

void ApplyStatusTooltip(QWidget* widget, QLabel* label,
                        oclero::qlementine::StatusBadgeWidget* badge,
                        const QString& tooltip) {
  if (widget != nullptr) {
    widget->setToolTip(tooltip);
  }
  if (label != nullptr) {
    label->setToolTip(tooltip);
  }
  if (badge != nullptr) {
    badge->setToolTip(tooltip);
  }
}

auto CompactBackendStatusText(backend::BackendConnectionState state) -> QString {
  switch (state) {
    case backend::BackendConnectionState::kLocalOnly:
      return QStringLiteral("Backend: local");
    case backend::BackendConnectionState::kChecking:
      return QStringLiteral("Backend: checking");
    case backend::BackendConnectionState::kReachable:
      return QStringLiteral("Backend: online");
    case backend::BackendConnectionState::kUnavailable:
      return QStringLiteral("Backend: offline");
  }

  return QStringLiteral("Backend: unknown");
}

auto CompactDocumentStatusText(const QString& message, bool is_error) -> QString {
  if (is_error || message.contains(QStringLiteral("error"), Qt::CaseInsensitive)) {
    return QStringLiteral("Document: error");
  }
  if (message.contains(QStringLiteral("Saving"), Qt::CaseInsensitive)) {
    return QStringLiteral("Document: saving");
  }
  if (message.contains(QStringLiteral("Saved"), Qt::CaseInsensitive)) {
    return QStringLiteral("Document: saved");
  }
  if (message.contains(QStringLiteral("local-only"), Qt::CaseInsensitive) ||
      message.contains(QStringLiteral("local only"), Qt::CaseInsensitive)) {
    return QStringLiteral("Document: local");
  }
  if (message.contains(QStringLiteral("unavailable"), Qt::CaseInsensitive)) {
    return QStringLiteral("Document: waiting");
  }

  return QStringLiteral("Document: ready");
}

auto CompactSyncStatusText(const sync::DocumentSyncSnapshot& snapshot) -> QString {
  switch (snapshot.state) {
    case sync::DocumentSyncState::kDisabled:
      return QStringLiteral("Sync: off");
    case sync::DocumentSyncState::kUnavailable:
      return QStringLiteral("Sync: waiting");
    case sync::DocumentSyncState::kReady:
      if (snapshot.initial_pull_active) {
        return QStringLiteral("Sync: syncing");
      }
      return QStringLiteral("Sync: ready");
    case sync::DocumentSyncState::kError:
      return QStringLiteral("Sync: error");
  }

  return QStringLiteral("Sync: unknown");
}

auto IsHighPrioritySaveHint(const QString& text) -> bool {
  return text.contains(QStringLiteral("Saving"), Qt::CaseInsensitive) ||
         text.contains(QStringLiteral("Save error"), Qt::CaseInsensitive);
}

auto CompactAuthHint(const auth::AuthSessionManager* auth) -> QString {
  if (auth == nullptr) {
    return {};
  }

  switch (auth->State()) {
    case auth::AuthSessionState::kRefreshing:
      return QStringLiteral("Refreshing session...");
    case auth::AuthSessionState::kAwaitingCallback:
      return QStringLiteral("Completing browser sign-in...");
    case auth::AuthSessionState::kSignedOut:
    case auth::AuthSessionState::kError: {
      const auto subtitle = auth->Subtitle();
      if (subtitle.contains(QStringLiteral("expired"), Qt::CaseInsensitive) ||
          subtitle.contains(QStringLiteral("refresh failed"), Qt::CaseInsensitive)) {
        return QStringLiteral("Session expired. Sign in again.");
      }
      return {};
    }
    case auth::AuthSessionState::kDisabled:
    case auth::AuthSessionState::kAuthenticated:
      return {};
  }

  return {};
}

auto MakeStatusWidget(const QString& initial_text, QWidget* parent)
    -> std::tuple<QWidget*, oclero::qlementine::StatusBadgeWidget*, QLabel*> {
  auto* container = new QWidget(parent);
  auto* layout = new QHBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  auto* badge = new oclero::qlementine::StatusBadgeWidget(
      oclero::qlementine::StatusBadge::Info, oclero::qlementine::StatusBadgeSize::Small,
      container);
  auto* label = new QLabel(initial_text, container);

  layout->addWidget(badge, 0, Qt::AlignVCenter);
  layout->addWidget(label, 0, Qt::AlignVCenter);

  return {container, badge, label};
}

}  // namespace cppwiki::gui::main_window_helpers