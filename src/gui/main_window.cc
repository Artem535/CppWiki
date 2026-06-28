#include "gui/main_window.h"

#include <QDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
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

namespace cppwiki {

namespace {

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

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  BuildUi();
}

MainWindow::~MainWindow() = default;

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
  if (context_ != nullptr && context_->auth_session_manager != nullptr) {
    connect(context_->auth_session_manager, &auth::AuthSessionManager::sessionChanged, this,
            [this]() { UpdateAuthCollaborationHint(); });
  }
  CreateInitialPage();
  UpdateBackendStatus();
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
  connect(edit_mode_switch_, &oclero::qlementine::Switch::toggled, this, [this](bool) {
    if (current_page_ != nullptr) {
      current_page_->ToggleEditMode();
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
  collaboration_layout->addWidget(edit_mode_widget, 0, Qt::AlignVCenter);
  collaboration_layout->addStretch(1);
  collaboration_layout->addWidget(presence_strip_widget_, 0, Qt::AlignVCenter);
  header_layout->addWidget(collaboration_panel_, 1);

  shell_layout_->addWidget(header_row, 0, 1);

  setCentralWidget(shell_widget_);
  statusBar()->addPermanentWidget(backend_refresh_button_);
  statusBar()->addPermanentWidget(document_status_widget_);
  statusBar()->addPermanentWidget(backend_status_widget_);
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
