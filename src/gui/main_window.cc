#include "gui/main_window.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QString>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/widgets/StatusBadgeWidget.hpp>
#include <oclero/qlementine/widgets/Switch.hpp>
#include <optional>

#include "app/app_context.h"
#include "app/application.h"
#include "app/application_stylesheet.h"
#include "app/program_settings.h"
#include "auth/auth_session_manager.h"
#include "backend/backend_client.h"
#include "core/constants.h"
#include "core/qt_string.h"
#include "gui/coming_soon_widget.h"
#include "gui/main_window_helpers.h"
#include "gui/merge/conflict_merge_dialog.h"
#include "gui/page.h"
#include "gui/page_helpers.h"
#include "gui/presence_strip_widget.h"
#include "gui/settings_dialog.h"
#include "gui/workspace_rail_widget.h"
#include "sync/sync_service.h"

namespace cppwiki {

using namespace gui::main_window_helpers;

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
    bootstrap_available_value_ = CreateValueRow(form, this, QStringLiteral("Bootstrap available"));
    backend_sync_value_ = CreateValueRow(form, this, QStringLiteral("Backend sync"));
    repository_attached_value_ = CreateValueRow(form, this, QStringLiteral("Repository attached"));
    repository_supports_sync_value_ =
        CreateValueRow(form, this, QStringLiteral("Repository supports sync"));
    gateway_url_value_ = CreateValueRow(form, this, QStringLiteral("Gateway URL"));
    database_value_ = CreateValueRow(form, this, QStringLiteral("Database"));
    auth_mode_value_ = CreateValueRow(form, this, QStringLiteral("Auth mode"));
    token_passthrough_value_ = CreateValueRow(form, this, QStringLiteral("Token passthrough"));
    principal_value_ = CreateValueRow(form, this, QStringLiteral("Principal"));
    principal_email_value_ = CreateValueRow(form, this, QStringLiteral("Principal email"));
    initial_pull_value_ = CreateValueRow(form, this, QStringLiteral("Initial pull"));
    hydrated_workspaces_value_ = CreateValueRow(form, this, QStringLiteral("Hydrated workspaces"));
    workspace_hydration_value_ = CreateValueRow(form, this, QStringLiteral("Workspace hydration"));
    roles_value_ = CreateValueRow(form, this, QStringLiteral("Roles"));
    groups_value_ = CreateValueRow(form, this, QStringLiteral("Groups"));
    channels_value_ = CreateValueRow(form, this, QStringLiteral("Channels"));
    pending_conflicts_value_ = CreateValueRow(form, this, QStringLiteral("Pending conflicts"));

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
             QStringLiteral("%1: %2").arg(
                 SyncLifecycleStateLabel(snapshot.repository_status.state),
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
    SetValue(backend_sync_value_, BoolLabel(snapshot.backend_sync_enabled, u"Allowed", u"Rejected"),
             !snapshot.backend_sync_enabled, !snapshot.backend_sync_enabled,
             snapshot.backend_sync_enabled);
    SetValue(repository_attached_value_,
             BoolLabel(snapshot.has_repository, u"Attached", u"Missing"), !snapshot.has_repository,
             !snapshot.has_repository, snapshot.has_repository);
    SetValue(repository_supports_sync_value_,
             BoolLabel(snapshot.repository_supports_sync, u"Yes", u"No"),
             !snapshot.repository_supports_sync, !snapshot.repository_supports_sync,
             snapshot.repository_supports_sync);
    SetValue(gateway_url_value_,
             snapshot.bootstrap.gateway_url.trimmed().isEmpty() ? QStringLiteral("Not provided")
                                                                : snapshot.bootstrap.gateway_url,
             false, snapshot.bootstrap.gateway_url.trimmed().isEmpty(),
             !snapshot.bootstrap.gateway_url.trimmed().isEmpty());
    SetValue(database_value_,
             snapshot.bootstrap.database_name.trimmed().isEmpty()
                 ? QStringLiteral("Not provided")
                 : snapshot.bootstrap.database_name,
             false, snapshot.bootstrap.database_name.trimmed().isEmpty(),
             !snapshot.bootstrap.database_name.trimmed().isEmpty());
    SetValue(auth_mode_value_,
             snapshot.bootstrap.auth_mode.trimmed().isEmpty() ? QStringLiteral("Not provided")
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
    SetValue(initial_pull_value_,
             snapshot.initial_pull_completed ? QStringLiteral("Completed")
             : snapshot.initial_pull_active  ? QStringLiteral("In progress")
                                             : QStringLiteral("Pending"),
             false, !snapshot.initial_pull_completed, snapshot.initial_pull_completed);
    SetValue(hydrated_workspaces_value_, JoinOrFallback(snapshot.hydrated_workspace_ids), false,
             snapshot.hydrated_workspace_ids.isEmpty(), !snapshot.hydrated_workspace_ids.isEmpty());
    SetValue(workspace_hydration_value_, BuildWorkspaceHydrationSummary(snapshot),
             snapshot.workspace_hydration.isEmpty(),
             snapshot.workspace_hydration.isEmpty() ||
                 (!snapshot.initial_pull_completed && !snapshot.initial_pull_active),
             snapshot.initial_pull_completed);
    SetValue(roles_value_, JoinOrFallback(snapshot.bootstrap.principal_roles), false,
             snapshot.bootstrap.principal_roles.isEmpty(),
             !snapshot.bootstrap.principal_roles.isEmpty());
    SetValue(groups_value_, JoinOrFallback(snapshot.bootstrap.principal_groups), false,
             snapshot.bootstrap.principal_groups.isEmpty(),
             !snapshot.bootstrap.principal_groups.isEmpty());
    SetValue(channels_value_, JoinOrFallback(snapshot.bootstrap.channels), false,
             snapshot.bootstrap.channels.isEmpty(), !snapshot.bootstrap.channels.isEmpty());
    SetValue(pending_conflicts_value_, QString::number(snapshot.conflict_count),
             snapshot.state == sync::DocumentSyncState::kError, snapshot.has_conflicts,
             snapshot.conflict_count == 0);
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
    label->setStyleSheet(
        QStringLiteral("color: %1;").arg(StateTextColor(is_error, is_warning, is_success)));
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
  QLabel* initial_pull_value_ = nullptr;
  QLabel* hydrated_workspaces_value_ = nullptr;
  QLabel* workspace_hydration_value_ = nullptr;
  QLabel* roles_value_ = nullptr;
  QLabel* groups_value_ = nullptr;
  QLabel* channels_value_ = nullptr;
  QLabel* pending_conflicts_value_ = nullptr;
};

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  BuildUi();
}

MainWindow::~MainWindow() = default;

void MainWindow::ApplyStylesheetToSafeDescendants(AccentColor accent_color) {
  // See main_window.h's comment on this method for the full story. In short: Qt's
  // QStyleSheetStyle wraps a widget's style() (even a style you explicitly assigned with
  // setStyle()) as soon as ANY ancestor up to the top-level window has a non-empty
  // styleSheet(); there is no public way to opt a single descendant out of that once an
  // ancestor is styled. So instead of styling MainWindow itself (which would poison
  // edit_mode_switch_'s style() and break its qlementine theme colors, since it's a
  // descendant), the stylesheet is applied individually to each widget below that is NOT
  // an ancestor of edit_mode_switch_. collaboration_panel_ IS such an ancestor and paints
  // its own background/border in code instead (see CollaborationPanelFrame).
  current_accent_color_ = accent_color;
  for (auto* target : {static_cast<QWidget*>(workspace_rail_),
                       static_cast<QWidget*>(presence_strip_widget_),
                       static_cast<QWidget*>(edit_mode_label_),
                       static_cast<QWidget*>(save_state_label_),
                       static_cast<QWidget*>(backend_refresh_button_),
                       document_status_widget_, backend_status_widget_, sync_status_widget_,
                       sync_conflicts_widget_, current_sidebar_widget_,
                       current_content_widget_}) {
    if (target != nullptr) {
      ApplyApplicationStylesheet(target, accent_color);
    }
  }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  if ((watched == sync_status_widget_ || (sync_status_widget_ != nullptr && watched != nullptr &&
                                          watched->parent() == sync_status_widget_)) &&
      event != nullptr && event->type() == QEvent::MouseButtonRelease) {
    ShowSyncDetailsDialog();
    return true;
  }

  if ((watched == sync_conflicts_widget_ ||
       (sync_conflicts_widget_ != nullptr && watched != nullptr &&
        watched->parent() == sync_conflicts_widget_)) &&
      event != nullptr && event->type() == QEvent::MouseButtonRelease) {
    ReopenConflictWindow();
    return true;
  }

  return QMainWindow::eventFilter(watched, event);
}

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
  connect(page, &Page::documentStatusChanged, this, [this](const QString& message, bool is_error) {
    UpdateDocumentStatus(message, is_error);
  });
  connect(page, &Page::collaborationStatusChanged, this,
          [this](const QString& summary, const QString& details, bool is_warning) {
            UpdateCollaborationStatus(summary, details, is_warning);
          });
  connect(page, &Page::editModeStateChanged, this, &MainWindow::UpdateEditModeUi);
  connect(page, &Page::documentConflictDetected, this,
          [this](const QString&, const QString& conflict_id) { ShowConflictWindow(conflict_id); });
  current_page_ = page;
  current_sidebar_widget_ = current_page_->SidebarWidget();
  current_content_widget_ = current_page_->ContentWidget();

  if (shell_layout_ != nullptr) {
    shell_layout_->addWidget(current_sidebar_widget_, 0, 0, 2, 1);
    shell_layout_->addWidget(current_content_widget_, 1, 1);
  }
  // current_sidebar_widget_/current_content_widget_ are siblings of header_row under
  // shell_widget_, not ancestors of edit_mode_switch_, so they're safe to style directly (see
  // ApplyStylesheetToSafeDescendants()'s comment). Re-applied here since Page (and therefore
  // these two widgets) is recreated each time CreateInitialPage() runs.
  ApplyStylesheetToSafeDescendants(current_accent_color_);
  setWindowTitle(QStringLiteral("CppWiki - %1").arg(current_page_->Title()));
  UpdateDocumentStatus(QStringLiteral("Document: ready"), false);
  UpdateCollaborationStatus(QStringLiteral("Collab: idle"),
                            QStringLiteral("Open a document to negotiate editing access."), false);
}

void MainWindow::ShowSettingsDialog() {
  if (context_ == nullptr) {
    return;
  }

  // Parented to `this` (MainWindow) so window managers see the standard transient-for
  // relationship a modal settings dialog should have — on tiling WMs (e.g. Hyprland) this is
  // what makes the dialog float over the tiled layout instead of being tiled in as its own
  // window (regressed when this was briefly unparented; see the fix for the report that this
  // dialog stopped floating). This is safe from the QStyleSheetStyle wrap that motivated the
  // original unparenting: MainWindow itself never carries a stylesheet at all (see
  // MainWindow::ApplyStylesheetToSafeDescendants()'s comment) — only specific safe-descendant
  // widgets do, and this dialog isn't one of them — so parenting it here doesn't put it in a
  // styled ancestor chain.
  gui::SettingsDialog dialog(context_->settings, this);
  dialog.setModal(true);
  if (!frameGeometry().isEmpty()) {
    const auto center = frameGeometry().center();
    dialog.move(center.x() - dialog.width() / 2, center.y() - dialog.height() / 2);
  }
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

namespace {

// Paints the background/border cppwiki.qss used to draw for QFrame#collaborationPanel,
// keyed off the same "collaborationState" dynamic property, but in code instead of QSS.
//
// collaboration_panel_ is a direct ancestor of edit_mode_switch_ (an
// oclero::qlementine::Switch, edit_mode_widget's sibling child), so it can never carry a
// stylesheet — see MainWindow::ApplyStylesheetToSafeDescendants()'s comment for why.
class CollaborationPanelFrame final : public QFrame {
 public:
  using QFrame::QFrame;

  // ADR-016: the "viewing" state's tint is the user's chosen accent color, not a fixed color
  // like the other three semantic states (editing/read-only/attention stay fixed regardless
  // of accent). Set by MainWindow::ApplyAccentColor().
  void SetAccentColor(AccentColor accent_color) {
    accent_color_ = accent_color;
    update();
  }

 protected:
  void paintEvent(QPaintEvent* /*event*/) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor background{255, 255, 255, 8};
    QColor border{255, 255, 255, 31};
    const auto state = property("collaborationState").toString();
    if (state == QStringLiteral("editing")) {
      background = QColor(28, 201, 156, 20);
      border = QColor(28, 201, 156, 82);
    } else if (state == QStringLiteral("viewing")) {
      const auto accent = AccentColorBaseColor(accent_color_);
      background = QColor(accent.red(), accent.green(), accent.blue(), 15);
      border = QColor(accent.red(), accent.green(), accent.blue(), 46);
    } else if (state == QStringLiteral("read-only")) {
      background = QColor(255, 193, 87, 20);
      border = QColor(255, 193, 87, 71);
    } else if (state == QStringLiteral("attention")) {
      background = QColor(255, 107, 107, 20);
      border = QColor(255, 107, 107, 71);
    }
    // "idle" and "local-only" both use the default background/border set above.

    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 14, 14);
    painter.fillPath(path, background);
    painter.setPen(QPen(border, 1));
    painter.drawPath(path);
  }

 private:
  AccentColor accent_color_ = AccentColor::kBlue;
};

}  // namespace

void MainWindow::ApplyAccentColor(AccentColor accent_color) {
  ApplyStylesheetToSafeDescendants(accent_color);
  if (auto* panel = static_cast<CollaborationPanelFrame*>(collaboration_panel_)) {
    panel->SetAccentColor(accent_color);
  }
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

  // header_row and collaboration_panel_ are both ancestors of edit_mode_switch_, so neither
  // may get cppwiki.qss applied (see ApplyStylesheetToSafeDescendants()'s comment). header_row
  // relies on QWidget's default (transparent) painting for the qss's old "background-color:
  // transparent" rule; collaboration_panel_ paints its own background/border in code (see
  // CollaborationPanelFrame above).
  auto* header_row = new QWidget(shell_widget_);
  auto* header_layout = new QHBoxLayout(header_row);
  header_layout->setContentsMargins(12, 12, 12, 8);
  header_layout->setSpacing(12);

  collaboration_panel_ = new CollaborationPanelFrame(header_row);
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
  std::tie(sync_conflicts_widget_, sync_conflicts_badge_, sync_conflicts_label_) =
      MakeStatusWidget(QStringLiteral("Conflicts: 0"), this);
  // MakeStatusWidget() doesn't set an objectName, so these had no cppwiki.qss rule at all and
  // fell back to the default (black, light-palette) QLabel text color against the dark status
  // bar. Name them so the rule added below actually applies.
  document_status_label_->setObjectName(QStringLiteral("statusLineLabel"));
  backend_status_label_->setObjectName(QStringLiteral("statusLineLabel"));
  sync_status_label_->setObjectName(QStringLiteral("statusLineLabel"));
  sync_conflicts_label_->setObjectName(QStringLiteral("statusLineLabel"));
  sync_status_widget_->setCursor(Qt::PointingHandCursor);
  sync_status_widget_->setToolTip(QStringLiteral("Open sync details"));
  sync_status_widget_->installEventFilter(this);
  for (auto* child : sync_status_widget_->findChildren<QWidget*>()) {
    child->installEventFilter(this);
  }
  sync_conflicts_widget_->setCursor(Qt::PointingHandCursor);
  sync_conflicts_widget_->setToolTip(QStringLiteral("Open the conflict resolution window"));
  sync_conflicts_widget_->installEventFilter(this);
  for (auto* child : sync_conflicts_widget_->findChildren<QWidget*>()) {
    child->installEventFilter(this);
  }
  sync_conflicts_widget_->hide();
  collaboration_layout->addWidget(edit_mode_widget, 0, Qt::AlignVCenter);
  collaboration_layout->addStretch(1);
  collaboration_layout->addWidget(sync_conflicts_widget_, 0, Qt::AlignVCenter);
  collaboration_layout->addWidget(presence_strip_widget_, 0, Qt::AlignVCenter);
  header_layout->addWidget(collaboration_panel_, 1);

  shell_layout_->addWidget(header_row, 0, 1);

  // The workspace rail (ADR-014) fully replaces the central content area per
  // mode. Documents mode is the pre-existing shell_widget_ (document tree +
  // Page), now one of N pages behind the mode stack rather than the only
  // thing MainWindow shows. AI Chat/Code are infrastructure-only placeholders
  // here; their real content is separate follow-up work (issues #29-#32).
  auto* root_widget = new QWidget(this);
  auto* root_layout = new QHBoxLayout(root_widget);
  root_layout->setContentsMargins(0, 0, 0, 0);
  root_layout->setSpacing(0);

  workspace_rail_ = new gui::WorkspaceRailWidget(root_widget);
  connect(workspace_rail_, &gui::WorkspaceRailWidget::modeSelected, this,
          &MainWindow::HandleModeSelected);
  root_layout->addWidget(workspace_rail_, 0);

  mode_stack_ = new QStackedWidget(root_widget);
  mode_stack_->addWidget(shell_widget_);
  ai_chat_page_ = new gui::ComingSoonWidget(
      QStringLiteral("AI Chat"),
      QStringLiteral("Coming soon: a conversational surface that can read, search and edit "
                     "wiki documents. See ADR-014/ADR-015."),
      mode_stack_);
  mode_stack_->addWidget(ai_chat_page_);
  code_page_ = new gui::ComingSoonWidget(
      QStringLiteral("Code"),
      QStringLiteral("Coming soon: an embedded coding agent working against a local "
                     "directory. See ADR-014/ADR-015."),
      mode_stack_);
  mode_stack_->addWidget(code_page_);
  mode_stack_->setCurrentWidget(shell_widget_);
  root_layout->addWidget(mode_stack_, 1);

  setCentralWidget(root_widget);
  statusBar()->addPermanentWidget(backend_refresh_button_);
  statusBar()->addPermanentWidget(document_status_widget_);
  statusBar()->addPermanentWidget(backend_status_widget_);
  statusBar()->addPermanentWidget(sync_status_widget_);
  menuBar()->hide();

  // Deliberately NOT calling ApplyStylesheetToSafeDescendants() here: at this point
  // current_accent_color_ is still its AccentColor::kBlue default (the real accent, loaded
  // from ProgramSettings, isn't known yet — Application::ReloadContext() applies it right
  // after MainWindow is constructed, before Application::Run() ever shows the window). Styling
  // with the wrong color here first and then immediately overwriting it before the window is
  // ever shown previously caused the workspace rail's accent tint to stick on the wrong
  // (first-applied) color after every restart — see ApplyApplicationStylesheet()'s
  // ForceStyleRefresh() comment for the underlying QStyleSheetStyle caching behavior. Leaving
  // these widgets unstyled for the brief window before ReloadContext() runs is fine: the window
  // isn't shown yet either way.
}

void MainWindow::HandleModeSelected(gui::WorkspaceRailWidget::Mode mode) {
  if (mode_stack_ == nullptr) {
    return;
  }
  switch (mode) {
    case gui::WorkspaceRailWidget::Mode::kDocuments:
      mode_stack_->setCurrentWidget(shell_widget_);
      break;
    case gui::WorkspaceRailWidget::Mode::kAiChat:
      mode_stack_->setCurrentWidget(ai_chat_page_);
      break;
    case gui::WorkspaceRailWidget::Mode::kCode:
      mode_stack_->setCurrentWidget(code_page_);
      break;
  }
}

void MainWindow::UpdateBackendStatus() {
  if (backend_status_label_ == nullptr || backend_status_badge_ == nullptr) {
    return;
  }

  if (context_ == nullptr || context_->backend_client == nullptr) {
    backend_status_label_->setText(QStringLiteral("Backend: unavailable"));
    backend_status_badge_->setBadge(oclero::qlementine::StatusBadge::Warning);
    ApplyStatusTooltip(backend_status_widget_, backend_status_label_, backend_status_badge_,
                       QStringLiteral("Backend client is not configured."));
    return;
  }

  const auto state = context_->backend_client->State();
  backend_status_label_->setText(CompactBackendStatusText(state));
  ApplyStatusTooltip(backend_status_widget_, backend_status_label_, backend_status_badge_,
                     context_->backend_client->StatusText());

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
  if (sync_status_label_ == nullptr || sync_status_badge_ == nullptr ||
      sync_conflicts_label_ == nullptr || sync_conflicts_badge_ == nullptr ||
      sync_conflicts_widget_ == nullptr) {
    return;
  }

  if (context_ == nullptr || context_->document_sync_service == nullptr) {
    sync_status_label_->setText(QStringLiteral("Sync: unavailable"));
    sync_status_badge_->setBadge(oclero::qlementine::StatusBadge::Warning);
    ApplyStatusTooltip(sync_status_widget_, sync_status_label_, sync_status_badge_,
                       QStringLiteral("Sync service is not configured."));
    sync_conflicts_widget_->hide();
    return;
  }

  const auto& snapshot = context_->document_sync_service->Snapshot();
  const auto state = snapshot.state;
  sync_status_label_->setText(CompactSyncStatusText(snapshot));
  ApplyStatusTooltip(sync_status_widget_, sync_status_label_, sync_status_badge_,
                     snapshot.status_text);

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

  const auto pending_conflicts = PendingConflicts(context_->document_repository);
  const auto pending_conflict_count = static_cast<qsizetype>(pending_conflicts.size());
  const auto snapshot_conflict_count = snapshot.conflict_count;
  const auto effective_conflict_count = snapshot_conflict_count > pending_conflict_count
                                            ? snapshot_conflict_count
                                            : pending_conflict_count;
  const auto has_pending_conflicts = pending_conflict_count > 0;
  const auto show_conflicts = snapshot.has_conflicts || has_pending_conflicts;

  if (show_conflicts) {
    sync_conflicts_label_->setText(
        QStringLiteral("Conflicts: %1").arg(QString::number(effective_conflict_count)));
    sync_conflicts_badge_->setBadge(oclero::qlementine::StatusBadge::Warning);
    const auto snapshot_matches_repository = snapshot.has_conflicts == has_pending_conflicts &&
                                             snapshot_conflict_count == pending_conflict_count;
    const auto tooltip =
        snapshot_matches_repository
            ? QStringLiteral("%1 pending conflict%2. Click to open the conflict window.")
                  .arg(QString::number(effective_conflict_count),
                       effective_conflict_count == 1 ? QString{} : QStringLiteral("s"))
            : QStringLiteral(
                  "Snapshot reports %1 conflict%2, repository currently has %3 pending conflict%4. "
                  "Click to open the conflict window.")
                  .arg(QString::number(snapshot_conflict_count),
                       snapshot_conflict_count == 1 ? QString{} : QStringLiteral("s"),
                       QString::number(pending_conflict_count),
                       pending_conflict_count == 1 ? QString{} : QStringLiteral("s"));
    ApplyStatusTooltip(sync_conflicts_widget_, sync_conflicts_label_, sync_conflicts_badge_,
                       tooltip);
    sync_conflicts_widget_->show();
  } else {
    sync_conflicts_widget_->hide();
  }

  RefreshSyncDetailsDialog();
}

void MainWindow::ShowSyncDetailsDialog() {
  if (sync_details_dialog_ == nullptr) {
    sync_details_dialog_ = new SyncDetailsDialog(this);
    connect(sync_details_dialog_, &QObject::destroyed, this,
            [this]() { sync_details_dialog_.clear(); });
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

void MainWindow::ShowConflictWindow(const QString& conflict_id) {
  if (context_ == nullptr || context_->document_repository == nullptr ||
      conflict_id.trimmed().isEmpty()) {
    return;
  }

  if (conflict_window_ != nullptr) {
    conflict_window_->close();
  }

  auto* dialog =
      new gui::merge::ConflictMergeDialog(context_->document_repository, conflict_id,
                                          gui::page_helpers::EffectiveAuthorId(*context_), this);
  dialog->setAttribute(Qt::WA_DeleteOnClose, true);
  dialog->setModal(false);
  connect(dialog, &gui::merge::ConflictMergeDialog::mergeApplied, this, [this]() {
    if (context_->document_sync_service != nullptr) {
      context_->document_sync_service->RefreshStatus();
    }
    RefreshSyncDetailsDialog();
    UpdateSyncStatus();
    if (current_page_ != nullptr) {
      current_page_->RefreshCurrentDocumentConflictState();
    }
  });
  connect(dialog, &QObject::destroyed, this, [this]() { conflict_window_.clear(); });
  conflict_window_ = dialog;
  dialog->show();
  dialog->raise();
  dialog->activateWindow();
}

void MainWindow::ReopenConflictWindow() {
  if (conflict_window_ != nullptr) {
    conflict_window_->show();
    conflict_window_->raise();
    conflict_window_->activateWindow();
    return;
  }

  if (context_ == nullptr || context_->document_repository == nullptr) {
    return;
  }

  const auto conflict = FirstPendingConflict(context_->document_repository);
  if (!conflict.has_value()) {
    return;
  }

  ShowConflictWindow(QString::fromStdString(conflict->id));
}

void MainWindow::UpdateDocumentStatus(const QString& message, bool is_error) {
  if (document_status_label_ == nullptr || document_status_badge_ == nullptr) {
    return;
  }

  document_status_label_->setText(CompactDocumentStatusText(message, is_error));
  document_status_label_->setStyleSheet(QString{});
  ApplyStatusTooltip(document_status_widget_, document_status_label_, document_status_badge_,
                     message);
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
        collaboration_panel_->update();
      }
      presence_strip_widget_->SetCollaborationState(QStringLiteral("editing"));
      fallback_editor_user_id_ = QStringLiteral("You");
      fallback_editor_is_self_ = true;
      presence_strip_widget_->SetEditor(QStringLiteral("You"), true);
      collaboration_hint_.clear();
    } else if (summary.contains(QStringLiteral("read-only"), Qt::CaseInsensitive)) {
      if (collaboration_panel_ != nullptr) {
        collaboration_panel_->setProperty("collaborationState", QStringLiteral("read-only"));
        collaboration_panel_->update();
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
        collaboration_panel_->update();
      }
      presence_strip_widget_->SetCollaborationState(QStringLiteral("local-only"));
      fallback_editor_user_id_.clear();
      fallback_editor_is_self_ = false;
      presence_strip_widget_->ClearEditor();
      collaboration_hint_.clear();
    } else if (summary.contains(QStringLiteral("lock lost"), Qt::CaseInsensitive) ||
               summary.contains(QStringLiteral("session expired"), Qt::CaseInsensitive) ||
               summary.contains(QStringLiteral("unavailable"), Qt::CaseInsensitive) || is_warning) {
      if (collaboration_panel_ != nullptr) {
        collaboration_panel_->setProperty("collaborationState", QStringLiteral("attention"));
        collaboration_panel_->update();
      }
      presence_strip_widget_->SetCollaborationState(QStringLiteral("attention"));
      fallback_editor_user_id_.clear();
      fallback_editor_is_self_ = false;
      presence_strip_widget_->ClearEditor();
      collaboration_hint_ = details.trimmed().isEmpty() ? summary : details;
    } else {
      if (collaboration_panel_ != nullptr) {
        collaboration_panel_->setProperty("collaborationState", QStringLiteral("viewing"));
        collaboration_panel_->update();
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
