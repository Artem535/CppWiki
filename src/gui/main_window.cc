#include "gui/main_window.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QSettings>
#include <QStatusBar>
#include <QString>
#include <QToolButton>
#include <QWidget>

#include <oclero/qlementine/widgets/StatusBadgeWidget.hpp>

#include <tuple>

#include "app/app_context.h"
#include "app/program_settings.h"
#include "backend/backend_client.h"
#include "core/constants.h"
#include "gui/i_page.h"
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
  }
  CreateInitialPage();
  UpdateBackendStatus();
}

void MainWindow::CreateInitialPage() {
  if (context_ == nullptr) {
    setCentralWidget(nullptr);
    setWindowTitle(QStringLiteral("CppWiki"));
    UpdateDocumentStatus(QStringLiteral("Document: unavailable"), true);
    return;
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
  current_page_ = page;

  setCentralWidget(current_page_->Widget());
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
  std::tie(document_status_widget_, document_status_badge_, document_status_label_) =
      MakeStatusWidget(QStringLiteral("Document: ready"), this);
  std::tie(collaboration_status_widget_, collaboration_status_badge_, collaboration_status_label_) =
      MakeStatusWidget(QStringLiteral("Collab: idle"), this);
  std::tie(backend_status_widget_, backend_status_badge_, backend_status_label_) =
      MakeStatusWidget(QStringLiteral("Backend: local only"), this);
  statusBar()->addPermanentWidget(backend_refresh_button_);
  statusBar()->addPermanentWidget(document_status_widget_);
  statusBar()->addPermanentWidget(collaboration_status_widget_);
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
  if (collaboration_status_label_ == nullptr || collaboration_status_badge_ == nullptr) {
    return;
  }

  collaboration_status_label_->setText(
      details.trimmed().isEmpty() ? summary : QStringLiteral("%1 (%2)").arg(summary, details));
  collaboration_status_label_->setToolTip(details);

  if (summary.contains(QStringLiteral("editing"), Qt::CaseInsensitive)) {
    collaboration_status_badge_->setBadge(oclero::qlementine::StatusBadge::Success);
    return;
  }

  if (summary.contains(QStringLiteral("local only"), Qt::CaseInsensitive)) {
    collaboration_status_badge_->setBadge(oclero::qlementine::StatusBadge::Info);
    return;
  }

  if (is_warning || summary.contains(QStringLiteral("read-only"), Qt::CaseInsensitive)) {
    collaboration_status_badge_->setBadge(oclero::qlementine::StatusBadge::Warning);
    return;
  }

  collaboration_status_badge_->setBadge(oclero::qlementine::StatusBadge::Info);
}

}  // namespace cppwiki
