#include "gui/main_window.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QMenuBar>
#include <QSettings>
#include <QStatusBar>
#include <QString>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <oclero/qlementine/widgets/StatusBadgeWidget.hpp>
#include <oclero/qlementine/widgets/Switch.hpp>

#include <tuple>

#include "app/app_context.h"
#include "app/program_settings.h"
#include "auth/auth_session_manager.h"
#include "backend/backend_client.h"
#include "core/constants.h"
#include "core/qt_string.h"
#include "gui/presence_strip_widget.h"
#include "gui/settings_dialog.h"
#include "gui/page.h"
#include "sync/sync_service.h"

namespace cppwiki {

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

auto BoolLabel(bool value, QStringView true_text = u"Yes", QStringView false_text = u"No")
    -> QString;
auto SyncLifecycleStateLabel(storage::SyncLifecycleState state) -> QString;
auto JoinOrFallback(const QStringList& values,
                    const QString& fallback = QStringLiteral("None")) -> QString;
auto BuildSyncGuidance(const sync::DocumentSyncSnapshot& snapshot) -> QString;

class SyncDetailsDialog final : public QDialog {
 public:
  explicit SyncDetailsDialog(QWidget* parent = nullptr) : QDialog(parent) {
    setWindowTitle(QStringLiteral("Sync details"));
    resize(640, 420);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    summary_label_ = new QLabel(this);
    summary_label_->setWordWrap(true);
    summary_label_->setObjectName(QStringLiteral("syncDetailsSummary"));
    layout->addWidget(summary_label_);

    auto* form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(8);
    layout->addLayout(form);

    overall_status_value_ = CreateValueRow(form, this, QStringLiteral("Overall status"));
    repository_state_value_ = CreateValueRow(form, this, QStringLiteral("Repository state"));
    auth_enabled_value_ = CreateValueRow(form, this, QStringLiteral("Auth enabled"));
    sync_enabled_value_ = CreateValueRow(form, this, QStringLiteral("Sync enabled"));
    access_token_value_ = CreateValueRow(form, this, QStringLiteral("Access token"));
    bootstrap_available_value_ =
        CreateValueRow(form, this, QStringLiteral("Bootstrap available"));
    backend_sync_value_ = CreateValueRow(form, this, QStringLiteral("Backend sync"));
    repository_attached_value_ =
        CreateValueRow(form, this, QStringLiteral("Repository attached"));
    repository_supports_sync_value_ =
        CreateValueRow(form, this, QStringLiteral("Repository supports sync"));
    gateway_url_value_ = CreateValueRow(form, this, QStringLiteral("Gateway URL"));
    database_value_ = CreateValueRow(form, this, QStringLiteral("Database"));
    auth_mode_value_ = CreateValueRow(form, this, QStringLiteral("Auth mode"));
    token_passthrough_value_ = CreateValueRow(form, this, QStringLiteral("Token passthrough"));
    principal_value_ = CreateValueRow(form, this, QStringLiteral("Principal"));
    principal_email_value_ = CreateValueRow(form, this, QStringLiteral("Principal email"));
    roles_value_ = CreateValueRow(form, this, QStringLiteral("Roles"));
    groups_value_ = CreateValueRow(form, this, QStringLiteral("Groups"));
    channels_value_ = CreateValueRow(form, this, QStringLiteral("Channels"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
  }

  void UpdateFromSnapshot(const sync::DocumentSyncSnapshot& snapshot) {
    const auto is_error = snapshot.state == sync::DocumentSyncState::kError;
    const auto is_warning = snapshot.state == sync::DocumentSyncState::kUnavailable;
    const auto is_success = snapshot.state == sync::DocumentSyncState::kReady;
    summary_label_->setText(BuildSyncGuidance(snapshot));
    summary_label_->setStyleSheet(QStringLiteral("font-weight: 600;"));

    SetValue(overall_status_value_, snapshot.status_text, is_error, is_warning, is_success);
    SetValue(repository_state_value_,
             QStringLiteral("%1: %2")
                 .arg(SyncLifecycleStateLabel(snapshot.repository_status.state),
                      QString::fromStdString(snapshot.repository_status.status_text)),
             snapshot.repository_status.state == storage::SyncLifecycleState::kError,
             snapshot.repository_status.state == storage::SyncLifecycleState::kDisabled,
             snapshot.repository_status.state == storage::SyncLifecycleState::kRunning);
    SetValue(auth_enabled_value_, BoolLabel(snapshot.auth_enabled), false, !snapshot.auth_enabled,
             snapshot.auth_enabled);
    SetValue(sync_enabled_value_, BoolLabel(snapshot.sync_enabled), false, !snapshot.sync_enabled,
             snapshot.sync_enabled);
    SetValue(access_token_value_, BoolLabel(snapshot.has_access_token, u"Present", u"Missing"),
             !snapshot.has_access_token, !snapshot.has_access_token, snapshot.has_access_token);
    SetValue(bootstrap_available_value_,
             BoolLabel(snapshot.backend_bootstrap_available, u"Available", u"Missing"),
             !snapshot.backend_bootstrap_available, !snapshot.backend_bootstrap_available,
             snapshot.backend_bootstrap_available);
    SetValue(backend_sync_value_,
             BoolLabel(snapshot.backend_sync_enabled, u"Allowed", u"Rejected"),
             !snapshot.backend_sync_enabled, !snapshot.backend_sync_enabled,
             snapshot.backend_sync_enabled);
    SetValue(repository_attached_value_,
             BoolLabel(snapshot.has_repository, u"Attached", u"Missing"), !snapshot.has_repository,
             !snapshot.has_repository, snapshot.has_repository);
    SetValue(repository_supports_sync_value_,
             BoolLabel(snapshot.repository_supports_sync, u"Yes", u"No"),
             !snapshot.repository_supports_sync, !snapshot.repository_supports_sync,
             snapshot.repository_supports_sync);
    SetValue(gateway_url_value_, snapshot.bootstrap.gateway_url.trimmed().isEmpty()
                                     ? QStringLiteral("Not provided")
                                     : snapshot.bootstrap.gateway_url,
             false, snapshot.bootstrap.gateway_url.trimmed().isEmpty(),
             !snapshot.bootstrap.gateway_url.trimmed().isEmpty());
    SetValue(database_value_, snapshot.bootstrap.database_name.trimmed().isEmpty()
                                  ? QStringLiteral("Not provided")
                                  : snapshot.bootstrap.database_name,
             false, snapshot.bootstrap.database_name.trimmed().isEmpty(),
             !snapshot.bootstrap.database_name.trimmed().isEmpty());
    SetValue(auth_mode_value_, snapshot.bootstrap.auth_mode.trimmed().isEmpty()
                                   ? QStringLiteral("Not provided")
                                   : snapshot.bootstrap.auth_mode,
             false, snapshot.bootstrap.auth_mode.trimmed().isEmpty(),
             !snapshot.bootstrap.auth_mode.trimmed().isEmpty());
    SetValue(token_passthrough_value_,
             BoolLabel(snapshot.bootstrap.token_passthrough, u"Enabled", u"Disabled"),
             !snapshot.bootstrap.token_passthrough, !snapshot.bootstrap.token_passthrough,
             snapshot.bootstrap.token_passthrough);
    SetValue(principal_value_,
             snapshot.bootstrap.principal_username.trimmed().isEmpty()
                 ? QStringLiteral("Current user")
                 : snapshot.bootstrap.principal_username,
             false, false, true);
    SetValue(principal_email_value_,
             snapshot.bootstrap.principal_email.trimmed().isEmpty()
                 ? QStringLiteral("Not provided")
                 : snapshot.bootstrap.principal_email,
             false, snapshot.bootstrap.principal_email.trimmed().isEmpty(),
             !snapshot.bootstrap.principal_email.trimmed().isEmpty());
    SetValue(roles_value_, JoinOrFallback(snapshot.bootstrap.principal_roles), false,
             snapshot.bootstrap.principal_roles.isEmpty(),
             !snapshot.bootstrap.principal_roles.isEmpty());
    SetValue(groups_value_, JoinOrFallback(snapshot.bootstrap.principal_groups), false,
             snapshot.bootstrap.principal_groups.isEmpty(),
             !snapshot.bootstrap.principal_groups.isEmpty());
    SetValue(channels_value_, JoinOrFallback(snapshot.bootstrap.channels), false,
             snapshot.bootstrap.channels.isEmpty(), !snapshot.bootstrap.channels.isEmpty());
  }

 private:
  static auto CreateValueRow(QFormLayout* form, QWidget* parent, const QString& label_text)
      -> QLabel* {
    auto* value = new QLabel(parent);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    value->setWordWrap(true);
    form->addRow(label_text, value);
    return value;
  }

  static void SetValue(QLabel* label, const QString& text, bool is_error, bool is_warning,
                       bool is_success) {
    label->setText(text);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setStyleSheet(QStringLiteral("color: %1;").arg(StateTextColor(is_error, is_warning,
                                                                         is_success)));
  }

  QLabel* summary_label_ = nullptr;
  QLabel* overall_status_value_ = nullptr;
  QLabel* repository_state_value_ = nullptr;
  QLabel* auth_enabled_value_ = nullptr;
  QLabel* sync_enabled_value_ = nullptr;
  QLabel* access_token_value_ = nullptr;
  QLabel* bootstrap_available_value_ = nullptr;
  QLabel* backend_sync_value_ = nullptr;
  QLabel* repository_attached_value_ = nullptr;
  QLabel* repository_supports_sync_value_ = nullptr;
  QLabel* gateway_url_value_ = nullptr;
  QLabel* database_value_ = nullptr;
  QLabel* auth_mode_value_ = nullptr;
  QLabel* token_passthrough_value_ = nullptr;
  QLabel* principal_value_ = nullptr;
  QLabel* principal_email_value_ = nullptr;
  QLabel* roles_value_ = nullptr;
  QLabel* groups_value_ = nullptr;
  QLabel* channels_value_ = nullptr;
};

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
    return QStringLiteral("Repository replication failed after bootstrap was applied.");
  }

  return QStringLiteral("Sync prerequisites are satisfied.");
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  BuildUi();
}

MainWindow::~MainWindow() = default;

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  if ((watched == sync_status_widget_ ||
       (sync_status_widget_ != nullptr && watched != nullptr &&
        watched->parent() == sync_status_widget_)) &&
      event != nullptr &&
      event->type() == QEvent::MouseButtonRelease) {
    ShowSyncDetailsDialog();
    return true;
  }

  return QMainWindow::eventFilter(watched, event);
}

namespace {

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

}  // namespace

void MainWindow::SetContext(AppContext* context) {
  context_ = context;
  if (context_ != nullptr && context_->backend_client != nullptr) {
    connect(context_->backend_client, &backend::BackendClient::statusChanged, this,
            [this](backend::BackendConnectionState state, const QString& status_text) {
              UpdateBackendStatus();
              if (state == backend::BackendConnectionState::kUnavailable) {
                statusBar()->showMessage(status_text, 3000);
              }
            });
    connect(context_->backend_client, &backend::BackendClient::presenceUpdated, this,
            [this](const QString& editor_user_id, bool editor_is_self,
                   const QStringList& viewer_user_ids) {
              if (presence_strip_widget_ == nullptr) {
                return;
              }

              if (editor_user_id.trimmed().isEmpty()) {
                if (fallback_editor_user_id_.trimmed().isEmpty()) {
                  presence_strip_widget_->ClearEditor();
                } else {
                  presence_strip_widget_->SetEditor(fallback_editor_user_id_,
                                                   fallback_editor_is_self_);
                }
              } else {
                presence_strip_widget_->SetEditor(editor_user_id, editor_is_self);
              }
              presence_strip_widget_->SetViewers(viewer_user_ids);
            });
  }
  if (context_ != nullptr && context_->document_sync_service != nullptr) {
    connect(context_->document_sync_service, &sync::SyncService::snapshotChanged, this,
            [this](const sync::DocumentSyncSnapshot&) { UpdateSyncStatus(); });
  }
  if (context_ != nullptr && context_->auth_session_manager != nullptr) {
    connect(context_->auth_session_manager, &auth::AuthSessionManager::sessionChanged, this,
            [this]() { UpdateAuthCollaborationHint(); });
  }
  CreateInitialPage();
  UpdateBackendStatus();
  UpdateSyncStatus();
  UpdateAuthCollaborationHint();
}

void MainWindow::CreateInitialPage() {
  if (context_ == nullptr) {
    setWindowTitle(QStringLiteral("CppWiki"));
    UpdateDocumentStatus(QStringLiteral("Document: unavailable"), true);
    return;
  }

  if (current_sidebar_widget_ != nullptr && shell_layout_ != nullptr) {
    shell_layout_->removeWidget(current_sidebar_widget_);
    current_sidebar_widget_->deleteLater();
    current_sidebar_widget_ = nullptr;
  }
  if (current_content_widget_ != nullptr && shell_layout_ != nullptr) {
    shell_layout_->removeWidget(current_content_widget_);
    current_content_widget_->deleteLater();
    current_content_widget_ = nullptr;
  }

  // Create the main page with the application context
  auto* page = new Page(*context_, this);
  connect(page, &Page::settingsRequested, this, [this]() { ShowSettingsDialog(); });
  connect(page, &Page::documentStatusChanged, this,
          [this](const QString& message, bool is_error) { UpdateDocumentStatus(message, is_error); });
  connect(page, &Page::collaborationStatusChanged, this,
          [this](const QString& summary, const QString& details, bool is_warning) {
            UpdateCollaborationStatus(summary, details, is_warning);
          });
  connect(page, &Page::editModeStateChanged, this, &MainWindow::UpdateEditModeUi);
  current_page_ = page;
  current_sidebar_widget_ = current_page_->SidebarWidget();
  current_content_widget_ = current_page_->ContentWidget();

  if (shell_layout_ != nullptr) {
    shell_layout_->addWidget(current_sidebar_widget_, 0, 0, 2, 1);
    shell_layout_->addWidget(current_content_widget_, 1, 1);
  }
  setWindowTitle(QStringLiteral("CppWiki - %1").arg(current_page_->Title()));
  UpdateDocumentStatus(QStringLiteral("Document: ready"), false);
  UpdateCollaborationStatus(QStringLiteral("Collab: idle"),
                            QStringLiteral("Open a document to negotiate editing access."), false);
}

void MainWindow::ShowSettingsDialog() {
  if (context_ == nullptr) {
    return;
  }

  gui::SettingsDialog dialog(context_->settings, this);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  const auto updated_settings = dialog.BuildProgramSettings();
  QSettings settings;
  updated_settings.SaveToSettings(settings);
  settings.sync();

  context_->settings = updated_settings;
  UpdateBackendStatus();
  statusBar()->showMessage(QStringLiteral("Settings saved."), 3000);
  emit settingsChanged();
}

void MainWindow::BuildUi() {
  setWindowTitle(QStringLiteral("CppWiki"));
  resize(constants::kInitialWindowWidth, constants::kInitialWindowHeight);
  statusBar()->showMessage(QStringLiteral("Ready"));

  shell_widget_ = new QWidget(this);
  shell_layout_ = new QGridLayout(shell_widget_);
  shell_layout_->setContentsMargins(0, 0, 0, 0);
  shell_layout_->setSpacing(0);
  shell_layout_->setColumnStretch(0, 0);
  shell_layout_->setColumnStretch(1, 1);
  shell_layout_->setRowStretch(0, 0);
  shell_layout_->setRowStretch(1, 1);

  auto* header_row = new QWidget(shell_widget_);
  header_row->setObjectName(QStringLiteral("shellHeaderRow"));
  auto* header_layout = new QHBoxLayout(header_row);
  header_layout->setContentsMargins(12, 12, 12, 8);
  header_layout->setSpacing(12);

  collaboration_panel_ = new QFrame(header_row);
  collaboration_panel_->setObjectName(QStringLiteral("collaborationPanel"));
  collaboration_panel_->setProperty("collaborationState", QStringLiteral("idle"));
  auto* collaboration_layout = new QHBoxLayout(collaboration_panel_);
  collaboration_layout->setContentsMargins(12, 6, 12, 6);
  collaboration_layout->setSpacing(10);

  auto* edit_mode_widget = new QWidget(collaboration_panel_);
  auto* edit_mode_layout = new QHBoxLayout(edit_mode_widget);
  edit_mode_layout->setContentsMargins(0, 0, 0, 0);
  edit_mode_layout->setSpacing(6);
  edit_mode_label_ = new QLabel(QStringLiteral("No document selected"), edit_mode_widget);
  edit_mode_label_->setObjectName(QStringLiteral("editModeLabel"));
  edit_mode_layout->addWidget(edit_mode_label_, 0, Qt::AlignVCenter);
  edit_mode_switch_ = new oclero::qlementine::Switch(edit_mode_widget);
  edit_mode_switch_->setObjectName(QStringLiteral("editModeSwitch"));
  edit_mode_switch_->setEnabled(false);
  connect(edit_mode_switch_, &oclero::qlementine::Switch::toggled, this, [this](bool checked) {
    if (current_page_ != nullptr) {
      current_page_->SetEditModeEnabled(checked);
    }
  });
  edit_mode_layout->addWidget(edit_mode_switch_, 0, Qt::AlignVCenter);
  save_state_label_ = new QLabel(QStringLiteral(""), edit_mode_widget);
  save_state_label_->setObjectName(QStringLiteral("saveStateLabel"));
  save_state_label_->hide();
  edit_mode_layout->addWidget(save_state_label_, 0, Qt::AlignVCenter);

  backend_refresh_button_ = new QToolButton(this);
  backend_refresh_button_->setObjectName(QStringLiteral("statusLineButton"));
  backend_refresh_button_->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
  backend_refresh_button_->setToolTip(QStringLiteral("Check backend now"));
  backend_refresh_button_->setAutoRaise(true);
  connect(backend_refresh_button_, &QToolButton::clicked, this, [this]() {
    if (context_ != nullptr && context_->backend_client != nullptr) {
      context_->backend_client->RefreshHealth();
    }
  });
  presence_strip_widget_ = new gui::PresenceStripWidget(collaboration_panel_);
  std::tie(document_status_widget_, document_status_badge_, document_status_label_) =
      MakeStatusWidget(QStringLiteral("Document: ready"), this);
  std::tie(backend_status_widget_, backend_status_badge_, backend_status_label_) =
      MakeStatusWidget(QStringLiteral("Backend: local only"), this);
  std::tie(sync_status_widget_, sync_status_badge_, sync_status_label_) =
      MakeStatusWidget(QStringLiteral("Sync: disabled"), this);
  sync_status_widget_->setCursor(Qt::PointingHandCursor);
  sync_status_widget_->setToolTip(QStringLiteral("Open sync details"));
  sync_status_widget_->installEventFilter(this);
  for (auto* child : sync_status_widget_->findChildren<QWidget*>()) {
    child->installEventFilter(this);
  }
  collaboration_layout->addWidget(edit_mode_widget, 0, Qt::AlignVCenter);
  collaboration_layout->addStretch(1);
  collaboration_layout->addWidget(presence_strip_widget_, 0, Qt::AlignVCenter);
  header_layout->addWidget(collaboration_panel_, 1);

  shell_layout_->addWidget(header_row, 0, 1);

  setCentralWidget(shell_widget_);
  statusBar()->addPermanentWidget(backend_refresh_button_);
  statusBar()->addPermanentWidget(document_status_widget_);
  statusBar()->addPermanentWidget(backend_status_widget_);
  statusBar()->addPermanentWidget(sync_status_widget_);
  menuBar()->hide();
}

void MainWindow::UpdateBackendStatus() {
  if (backend_status_label_ == nullptr || backend_status_badge_ == nullptr) {
    return;
  }

  if (context_ == nullptr || context_->backend_client == nullptr) {
    backend_status_label_->setText(QStringLiteral("Backend: unavailable"));
    backend_status_badge_->setBadge(oclero::qlementine::StatusBadge::Warning);
    return;
  }

  const auto state = context_->backend_client->State();
  backend_status_label_->setText(context_->backend_client->StatusText());

  switch (state) {
    case backend::BackendConnectionState::kLocalOnly:
      backend_status_badge_->setBadge(oclero::qlementine::StatusBadge::Info);
      break;

    case backend::BackendConnectionState::kChecking:
      backend_status_badge_->setBadge(oclero::qlementine::StatusBadge::Warning);
      break;

    case backend::BackendConnectionState::kReachable:
      backend_status_badge_->setBadge(oclero::qlementine::StatusBadge::Success);
      break;

    case backend::BackendConnectionState::kUnavailable:
      backend_status_badge_->setBadge(oclero::qlementine::StatusBadge::Error);
      break;
  }
}

void MainWindow::UpdateSyncStatus() {
  if (sync_status_label_ == nullptr || sync_status_badge_ == nullptr) {
    return;
  }

  if (context_ == nullptr || context_->document_sync_service == nullptr) {
    sync_status_label_->setText(QStringLiteral("Sync: unavailable"));
    sync_status_badge_->setBadge(oclero::qlementine::StatusBadge::Warning);
    return;
  }

  const auto& snapshot = context_->document_sync_service->Snapshot();
  const auto state = snapshot.state;
  sync_status_label_->setText(snapshot.status_text);

  switch (state) {
    case sync::DocumentSyncState::kDisabled:
      sync_status_badge_->setBadge(oclero::qlementine::StatusBadge::Info);
      break;
    case sync::DocumentSyncState::kUnavailable:
      sync_status_badge_->setBadge(oclero::qlementine::StatusBadge::Warning);
      break;
    case sync::DocumentSyncState::kReady:
      sync_status_badge_->setBadge(oclero::qlementine::StatusBadge::Success);
      break;
    case sync::DocumentSyncState::kError:
      sync_status_badge_->setBadge(oclero::qlementine::StatusBadge::Error);
      break;
  }

  RefreshSyncDetailsDialog();
}

void MainWindow::ShowSyncDetailsDialog() {
  if (sync_details_dialog_ == nullptr) {
    sync_details_dialog_ = new SyncDetailsDialog(this);
    connect(sync_details_dialog_, &QObject::destroyed, this, [this]() {
      sync_details_dialog_.clear();
    });
  }

  RefreshSyncDetailsDialog();
  sync_details_dialog_->show();
  sync_details_dialog_->raise();
  sync_details_dialog_->activateWindow();
}

void MainWindow::RefreshSyncDetailsDialog() {
  if (sync_details_dialog_ == nullptr) {
    return;
  }

  if (context_ == nullptr || context_->document_sync_service == nullptr) {
    sync_details_dialog_->UpdateFromSnapshot(sync::DocumentSyncSnapshot{});
    return;
  }

  sync_details_dialog_->UpdateFromSnapshot(context_->document_sync_service->Snapshot());
}

void MainWindow::UpdateDocumentStatus(const QString& message, bool is_error) {
  if (document_status_label_ == nullptr || document_status_badge_ == nullptr) {
    return;
  }

  document_status_label_->setText(message);
  document_status_label_->setStyleSheet(QString{});
  if (message.contains(QStringLiteral("Saving"), Qt::CaseInsensitive) ||
      message.contains(QStringLiteral("Saved"), Qt::CaseInsensitive) ||
      message.contains(QStringLiteral("Save error"), Qt::CaseInsensitive)) {
    save_state_hint_ = message;
  } else {
    save_state_hint_.clear();
  }
  RefreshCollaborationSecondaryText();

  if (is_error) {
    document_status_badge_->setBadge(oclero::qlementine::StatusBadge::Error);
    return;
  }

  if (message.contains(QStringLiteral("Saved"), Qt::CaseInsensitive)) {
    document_status_badge_->setBadge(oclero::qlementine::StatusBadge::Success);
    return;
  }

  if (message.contains(QStringLiteral("unavailable"), Qt::CaseInsensitive)) {
    document_status_badge_->setBadge(oclero::qlementine::StatusBadge::Warning);
    return;
  }

  document_status_badge_->setBadge(oclero::qlementine::StatusBadge::Info);
}

void MainWindow::UpdateCollaborationStatus(const QString& summary, const QString& details,
                                           bool is_warning) {
  if (presence_strip_widget_ != nullptr) {
    presence_strip_widget_->setToolTip(
        details.trimmed().isEmpty() ? summary : QStringLiteral("%1\n%2").arg(summary, details));
  }
  if (collaboration_panel_ != nullptr) {
    collaboration_panel_->setToolTip(
        details.trimmed().isEmpty() ? summary : QStringLiteral("%1\n%2").arg(summary, details));
  }

  if (presence_strip_widget_ != nullptr) {
    if (summary.contains(QStringLiteral("editing"), Qt::CaseInsensitive)) {
      if (collaboration_panel_ != nullptr) {
        collaboration_panel_->setProperty("collaborationState", QStringLiteral("editing"));
        collaboration_panel_->style()->unpolish(collaboration_panel_);
        collaboration_panel_->style()->polish(collaboration_panel_);
      }
      presence_strip_widget_->SetCollaborationState(QStringLiteral("editing"));
      fallback_editor_user_id_ = QStringLiteral("You");
      fallback_editor_is_self_ = true;
      presence_strip_widget_->SetEditor(QStringLiteral("You"), true);
      collaboration_hint_.clear();
    } else if (summary.contains(QStringLiteral("read-only"), Qt::CaseInsensitive)) {
      if (collaboration_panel_ != nullptr) {
        collaboration_panel_->setProperty("collaborationState", QStringLiteral("read-only"));
        collaboration_panel_->style()->unpolish(collaboration_panel_);
        collaboration_panel_->style()->polish(collaboration_panel_);
      }
      presence_strip_widget_->SetCollaborationState(QStringLiteral("read-only"));
      auto owner = details.trimmed();
      if (owner.startsWith(QStringLiteral("Locked by "), Qt::CaseInsensitive)) {
        owner.remove(0, QStringLiteral("Locked by ").size());
      }
      fallback_editor_user_id_ = owner;
      fallback_editor_is_self_ = false;
      presence_strip_widget_->SetEditor(owner, false);
      collaboration_hint_ = details.trimmed();
    } else if (summary.contains(QStringLiteral("local only"), Qt::CaseInsensitive)) {
      if (collaboration_panel_ != nullptr) {
        collaboration_panel_->setProperty("collaborationState", QStringLiteral("local-only"));
        collaboration_panel_->style()->unpolish(collaboration_panel_);
        collaboration_panel_->style()->polish(collaboration_panel_);
      }
      presence_strip_widget_->SetCollaborationState(QStringLiteral("local-only"));
      fallback_editor_user_id_.clear();
      fallback_editor_is_self_ = false;
      presence_strip_widget_->ClearEditor();
      collaboration_hint_.clear();
    } else if (summary.contains(QStringLiteral("lock lost"), Qt::CaseInsensitive) ||
               summary.contains(QStringLiteral("session expired"), Qt::CaseInsensitive) ||
               summary.contains(QStringLiteral("unavailable"), Qt::CaseInsensitive) ||
               is_warning) {
      if (collaboration_panel_ != nullptr) {
        collaboration_panel_->setProperty("collaborationState", QStringLiteral("attention"));
        collaboration_panel_->style()->unpolish(collaboration_panel_);
        collaboration_panel_->style()->polish(collaboration_panel_);
      }
      presence_strip_widget_->SetCollaborationState(QStringLiteral("attention"));
      fallback_editor_user_id_.clear();
      fallback_editor_is_self_ = false;
      presence_strip_widget_->ClearEditor();
      collaboration_hint_ = details.trimmed().isEmpty() ? summary : details;
    } else {
      if (collaboration_panel_ != nullptr) {
        collaboration_panel_->setProperty("collaborationState", QStringLiteral("viewing"));
        collaboration_panel_->style()->unpolish(collaboration_panel_);
        collaboration_panel_->style()->polish(collaboration_panel_);
      }
      presence_strip_widget_->SetCollaborationState(QStringLiteral("viewing"));
      fallback_editor_user_id_.clear();
      fallback_editor_is_self_ = false;
      presence_strip_widget_->ClearEditor();
      collaboration_hint_.clear();
    }
  }

  RefreshCollaborationSecondaryText();
}

void MainWindow::UpdateEditModeUi(const QString& label, bool checked, bool enabled) {
  if (edit_mode_label_ != nullptr) {
    edit_mode_label_->setText(label);
  }
  if (edit_mode_switch_ != nullptr) {
    const auto blocked = edit_mode_switch_->blockSignals(true);
    edit_mode_switch_->setChecked(checked);
    edit_mode_switch_->setEnabled(enabled);
    edit_mode_switch_->blockSignals(blocked);
  }
}

void MainWindow::UpdateAuthCollaborationHint() {
  auth_hint_ = CompactAuthHint(context_ != nullptr ? context_->auth_session_manager : nullptr);
  RefreshCollaborationSecondaryText();
}

void MainWindow::RefreshCollaborationSecondaryText() {
  if (save_state_label_ == nullptr) {
    return;
  }

  if (IsHighPrioritySaveHint(save_state_hint_)) {
    save_state_label_->setText(save_state_hint_);
    save_state_label_->show();
    return;
  }

  if (!auth_hint_.isEmpty()) {
    save_state_label_->setText(auth_hint_);
    save_state_label_->show();
    return;
  }

  if (!collaboration_hint_.isEmpty()) {
    save_state_label_->setText(collaboration_hint_);
    save_state_label_->show();
    return;
  }

  save_state_label_->setText(save_state_hint_);
  save_state_label_->setVisible(!save_state_hint_.isEmpty());
}

}  // namespace cppwiki
