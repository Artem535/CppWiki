#include "gui/settings_dialog.h"

#include <QAction>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/widgets/ActionButton.hpp>
#include <oclero/qlementine/widgets/Label.hpp>
#include <oclero/qlementine/widgets/LineEdit.hpp>
#include <oclero/qlementine/widgets/SegmentedControl.hpp>

#include "auth/ai_api_key_store.h"
#include "core/constants.h"
#include "core/qt_string.h"
#include "app/application.h"

namespace cppwiki::gui {

namespace {

auto MakeReadOnlyPathLineEdit(const QString& value, QWidget* parent)
    -> oclero::qlementine::LineEdit* {
  auto* edit = new oclero::qlementine::LineEdit(parent);
  edit->setText(value);
  edit->setReadOnly(true);
  edit->setClearButtonEnabled(false);
  return edit;
}

auto MakeSectionPage(QWidget* parent, QFormLayout** out_form_layout) -> QWidget* {
  auto* page = new QWidget(parent);
  auto* form_layout = new QFormLayout(page);
  form_layout->setLabelAlignment(Qt::AlignLeft);
  form_layout->setFormAlignment(Qt::AlignTop);
  form_layout->setHorizontalSpacing(12);
  form_layout->setVerticalSpacing(10);
  *out_form_layout = form_layout;
  return page;
}

}  // namespace

SettingsDialog::SettingsDialog(const ProgramSettings& settings, QWidget* parent)
    : QDialog(parent), current_settings_(settings) {
  // Callers should avoid passing a QWidget parent whose subtree has its own style sheet applied
  // (e.g. MainWindow — see ApplyApplicationStylesheet()): Qt cascades a style sheet down the
  // QObject parent/child tree to every descendant, including dialogs that only pop up as their
  // own top-level window because a parent was supplied for ownership/centering. That would wrap
  // this dialog's (and SegmentedControl's) QStyle in an internal QStyleSheetStyle proxy, which
  // breaks qobject_cast<QlementineStyle*>(style()) inside AbstractItemListWidget/SegmentedControl
  // the same way an app-wide qApp->setStyleSheet() would — confirmed empirically that even giving
  // this dialog its own explicit *empty* style sheet does not prevent that wrap, it only
  // neutralizes the inherited rules. See MainWindow::ShowSettingsDialog() for how this is
  // constructed (parent == nullptr, centered manually) to avoid it.
  setWindowTitle(QStringLiteral("Settings"));
  setModal(true);
  resize(680, 460);

  auto* root_layout = new QVBoxLayout(this);
  root_layout->setContentsMargins(16, 16, 16, 16);
  root_layout->setSpacing(14);

  auto* title = new oclero::qlementine::Label(QStringLiteral("Application settings"),
                                              oclero::qlementine::TextRole::H2, this);
  root_layout->addWidget(title);

  auto* hint = new oclero::qlementine::Label(
      QStringLiteral("Adjust desktop appearance, inspect local storage and prepare optional "
                     "backend and auth settings."),
      oclero::qlementine::TextRole::Caption, this);
  hint->setWordWrap(true);
  root_layout->addWidget(hint);

  section_control_ = new oclero::qlementine::SegmentedControl(this);
  // AbstractItemListWidget (SegmentedControl's base) paints itself entirely by hand and reads
  // theme colors/fonts via qobject_cast<QlementineStyle*>(style()). As long as this dialog isn't
  // parented into a style-sheet-wrapped subtree (see the constructor comment above), style()
  // already returns the real QlementineStyle here. This explicit setStyle() call is kept as
  // defense-in-depth in case that ever changes.
  if (auto* qlementine_style = cppwiki::GetQlementineStyle()) {
    section_control_->setStyle(qlementine_style);
  }
  section_control_->addItem(QStringLiteral("General"));
  section_control_->addItem(QStringLiteral("Backend & Sync"));
  section_control_->addItem(QStringLiteral("Auth"));
  section_control_->addItem(QStringLiteral("Collaboration (demo)"));
  section_control_->addItem(QStringLiteral("AI"));
  root_layout->addWidget(section_control_);

  section_stack_ = new QStackedWidget(this);
  root_layout->addWidget(section_stack_, 1);

  connect(section_control_, &oclero::qlementine::SegmentedControl::currentIndexChanged, this,
          [this]() { section_stack_->setCurrentIndex(section_control_->currentIndex()); });

  // General.
  QFormLayout* general_form_layout = nullptr;
  auto* general_page = MakeSectionPage(section_stack_, &general_form_layout);
  section_stack_->addWidget(general_page);

  font_size_spinbox_ = new QSpinBox(general_page);
  font_size_spinbox_->setRange(8, 24);
  font_size_spinbox_->setSuffix(QStringLiteral(" pt"));
  font_size_spinbox_->setValue(current_settings_.ApplicationFontPointSize());
  general_form_layout->addRow(QStringLiteral("Font size"), font_size_spinbox_);

  // Backend & Sync.
  QFormLayout* backend_form_layout = nullptr;
  auto* backend_page = MakeSectionPage(section_stack_, &backend_form_layout);
  section_stack_->addWidget(backend_page);

  backend_enabled_checkbox_ =
      new QCheckBox(QStringLiteral("Use backend when available"), backend_page);
  backend_enabled_checkbox_->setChecked(current_settings_.BackendEnabled());
  backend_form_layout->addRow(QStringLiteral("Backend"), backend_enabled_checkbox_);

  backend_base_url_edit_ = new oclero::qlementine::LineEdit(backend_page);
  backend_base_url_edit_->setText(current_settings_.BackendBaseUrl());
  backend_base_url_edit_->setPlaceholderText(QStringLiteral("http://127.0.0.1:8080"));
  backend_base_url_edit_->setEnabled(current_settings_.BackendEnabled());
  connect(backend_enabled_checkbox_, &QCheckBox::toggled, backend_base_url_edit_,
          &QWidget::setEnabled);
  backend_form_layout->addRow(QStringLiteral("Backend URL"), backend_base_url_edit_);

  sync_enabled_checkbox_ =
      new QCheckBox(QStringLiteral("Prepare authenticated document sync"), backend_page);
  sync_enabled_checkbox_->setChecked(current_settings_.SyncEnabled());
  backend_form_layout->addRow(QStringLiteral("Sync"), sync_enabled_checkbox_);

  database_directory_edit_ =
      MakeReadOnlyPathLineEdit(current_settings_.DatabaseDirectory(), backend_page);
  auto* open_folder_action = new QAction(QIcon::fromTheme(QStringLiteral("folder-open")),
                                         QStringLiteral("Open database folder"), this);
  auto* open_folder_button = new oclero::qlementine::ActionButton(backend_page);
  open_folder_button->setAction(open_folder_action);
  open_folder_button->setMinimumWidth(160);
  connect(open_folder_action, &QAction::triggered, this, [this]() {
    const auto path = database_directory_edit_->text().trimmed();
    if (!path.isEmpty()) {
      QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
  });
  auto* folder_row = new QWidget(backend_page);
  auto* folder_layout = new QHBoxLayout(folder_row);
  folder_layout->setContentsMargins(0, 0, 0, 0);
  folder_layout->setSpacing(8);
  folder_layout->addWidget(database_directory_edit_, 1);
  folder_layout->addWidget(open_folder_button, 0);
  backend_form_layout->addRow(QStringLiteral("Database folder"), folder_row);

  // Auth.
  QFormLayout* auth_form_layout = nullptr;
  auto* auth_page = MakeSectionPage(section_stack_, &auth_form_layout);
  section_stack_->addWidget(auth_page);

  auth_enabled_checkbox_ = new QCheckBox(QStringLiteral("Use desktop auth spike"), auth_page);
  auth_enabled_checkbox_->setChecked(current_settings_.AuthEnabled());
  auth_form_layout->addRow(QStringLiteral("Auth"), auth_enabled_checkbox_);

  auth_authorization_url_edit_ = new oclero::qlementine::LineEdit(auth_page);
  auth_authorization_url_edit_->setText(current_settings_.AuthAuthorizationUrl());
  auth_authorization_url_edit_->setPlaceholderText(
      QStringLiteral("https://auth.example/application/o/authorize/"));
  auth_authorization_url_edit_->setEnabled(current_settings_.AuthEnabled());
  auth_form_layout->addRow(QStringLiteral("Auth URL"), auth_authorization_url_edit_);

  auth_token_url_edit_ = new oclero::qlementine::LineEdit(auth_page);
  auth_token_url_edit_->setText(current_settings_.AuthTokenUrl());
  auth_token_url_edit_->setPlaceholderText(
      QStringLiteral("https://auth.example/application/o/token/"));
  auth_token_url_edit_->setEnabled(current_settings_.AuthEnabled());
  auth_form_layout->addRow(QStringLiteral("Auth token URL"), auth_token_url_edit_);

  auth_client_id_edit_ = new oclero::qlementine::LineEdit(auth_page);
  auth_client_id_edit_->setText(current_settings_.AuthClientId());
  auth_client_id_edit_->setPlaceholderText(QStringLiteral("cppwiki-desktop"));
  auth_client_id_edit_->setEnabled(current_settings_.AuthEnabled());
  auth_form_layout->addRow(QStringLiteral("Auth client ID"), auth_client_id_edit_);

  auth_redirect_uri_edit_ = new oclero::qlementine::LineEdit(auth_page);
  auth_redirect_uri_edit_->setText(current_settings_.AuthRedirectUri());
  auth_redirect_uri_edit_->setPlaceholderText(
      QStringLiteral("http://127.0.0.1:38080/auth/callback"));
  auth_redirect_uri_edit_->setEnabled(current_settings_.AuthEnabled());
  auth_form_layout->addRow(QStringLiteral("Auth redirect URI"), auth_redirect_uri_edit_);

  connect(auth_enabled_checkbox_, &QCheckBox::toggled, auth_authorization_url_edit_,
          &QWidget::setEnabled);
  connect(auth_enabled_checkbox_, &QCheckBox::toggled, auth_token_url_edit_, &QWidget::setEnabled);
  connect(auth_enabled_checkbox_, &QCheckBox::toggled, auth_client_id_edit_, &QWidget::setEnabled);
  connect(auth_enabled_checkbox_, &QCheckBox::toggled, auth_redirect_uri_edit_,
          &QWidget::setEnabled);

  // Collaboration (demo).
  QFormLayout* demo_form_layout = nullptr;
  auto* demo_page = MakeSectionPage(section_stack_, &demo_form_layout);
  section_stack_->addWidget(demo_page);

  demo_collaboration_enabled_checkbox_ =
      new QCheckBox(QStringLiteral("Override collaboration identity for local testing"), demo_page);
  demo_collaboration_enabled_checkbox_->setChecked(current_settings_.DemoCollaborationEnabled());
  demo_form_layout->addRow(QStringLiteral("Demo mode"), demo_collaboration_enabled_checkbox_);

  demo_collaboration_user_id_edit_ = new oclero::qlementine::LineEdit(demo_page);
  demo_collaboration_user_id_edit_->setText(current_settings_.DemoCollaborationUserId());
  demo_collaboration_user_id_edit_->setPlaceholderText(QStringLiteral("demo-alice"));
  demo_collaboration_user_id_edit_->setEnabled(current_settings_.DemoCollaborationEnabled());
  connect(demo_collaboration_enabled_checkbox_, &QCheckBox::toggled,
          demo_collaboration_user_id_edit_, &QWidget::setEnabled);
  demo_form_layout->addRow(QStringLiteral("Demo profile"), demo_collaboration_user_id_edit_);

  // AI.
  QFormLayout* ai_form_layout = nullptr;
  auto* ai_page = MakeSectionPage(section_stack_, &ai_form_layout);
  section_stack_->addWidget(ai_page);

  ai_features_enabled_checkbox_ = new QCheckBox(QStringLiteral("Enable AI features"), ai_page);
  ai_features_enabled_checkbox_->setChecked(current_settings_.AiFeaturesEnabled());
  ai_form_layout->addRow(QStringLiteral("AI features"), ai_features_enabled_checkbox_);

  ai_autocomplete_enabled_checkbox_ = new QCheckBox(QStringLiteral("Enable autocomplete"), ai_page);
  ai_autocomplete_enabled_checkbox_->setChecked(current_settings_.AiAutocompleteEnabled());
  ai_form_layout->addRow(QStringLiteral("Autocomplete"), ai_autocomplete_enabled_checkbox_);

  // Separate opt-in (issue #59): distinct from ai_features_enabled_checkbox_
  // above so users who want the toolbar/slash-menu AI features but find
  // inline suggestions distracting can disable just this one.
  ai_inline_suggestions_enabled_checkbox_ =
      new QCheckBox(QStringLiteral("Enable inline text suggestions"), ai_page);
  ai_inline_suggestions_enabled_checkbox_->setChecked(
      current_settings_.AiInlineSuggestionsEnabled());
  ai_form_layout->addRow(QStringLiteral("Inline suggestions"),
                         ai_inline_suggestions_enabled_checkbox_);

  // Local-key fallback (ADR-012 addendum): only relevant when no backend is
  // configured for this profile. The key is read from / written to the OS
  // keychain, never QSettings.
  ai_local_api_key_edit_ = new oclero::qlementine::LineEdit(ai_page);
  ai_local_api_key_edit_->setPlaceholderText(QStringLiteral("sk-..."));
  ai_local_api_key_edit_->setEchoMode(QLineEdit::Password);
  ai_form_layout->addRow(QStringLiteral("Local AI provider API key"), ai_local_api_key_edit_);

  ai_local_api_key_hint_ = new QLabel(
      QStringLiteral("Only used when no backend is configured (Backend & Sync tab). In that "
                     "mode, AI requests go directly from this machine to the AI provider, not "
                     "through cppwiki_server."),
      ai_page);
  ai_local_api_key_hint_->setWordWrap(true);
  ai_form_layout->addRow(QString{}, ai_local_api_key_hint_);

  const auto update_local_api_key_visibility = [this]() {
    const auto show_local_key = !backend_enabled_checkbox_->isChecked();
    ai_local_api_key_edit_->setVisible(show_local_key);
    ai_local_api_key_hint_->setVisible(show_local_key);
  };
  update_local_api_key_visibility();
  connect(backend_enabled_checkbox_, &QCheckBox::toggled, this, update_local_api_key_visibility);

  ai_api_key_store_ =
      std::make_unique<auth::AiApiKeyStore>(ToQString(constants::kApplicationName), this);
  connect(ai_api_key_store_.get(), &auth::AiApiKeyStore::apiKeyLoaded, this,
          [this](const QString& api_key) { ai_local_api_key_edit_->setText(api_key); });
  ai_api_key_store_->Load();

  section_control_->setCurrentIndex(0);
  section_stack_->setCurrentIndex(0);

  auto* buttons = new QDialogButtonBox(this);
  auto* cancel_button = buttons->addButton(QDialogButtonBox::Cancel);
  auto* save_button = buttons->addButton(QDialogButtonBox::Save);
  save_button->setDefault(true);

  connect(cancel_button, &QPushButton::clicked, this, &QDialog::reject);
  connect(save_button, &QPushButton::clicked, this, [this]() {
    const auto api_key = ai_local_api_key_edit_->text().trimmed();
    if (api_key.isEmpty()) {
      ai_api_key_store_->Clear();
    } else {
      ai_api_key_store_->Save(api_key);
    }
    accept();
  });
  root_layout->addWidget(buttons);
}

SettingsDialog::~SettingsDialog() = default;

auto SettingsDialog::BuildProgramSettings() const -> ProgramSettings {
  const auto backend_base_url = backend_base_url_edit_->text().trimmed().isEmpty()
                                    ? current_settings_.BackendBaseUrl()
                                    : backend_base_url_edit_->text().trimmed();
  const auto auth_authorization_url = auth_authorization_url_edit_->text().trimmed();
  const auto auth_token_url = auth_token_url_edit_->text().trimmed();
  const auto auth_client_id = auth_client_id_edit_->text().trimmed();
  const auto auth_redirect_uri = auth_redirect_uri_edit_->text().trimmed().isEmpty()
                                     ? current_settings_.AuthRedirectUri()
                                     : auth_redirect_uri_edit_->text().trimmed();
  const auto demo_collaboration_user_id = demo_collaboration_user_id_edit_->text().trimmed();

  return ProgramSettings(
      current_settings_.ApplicationName(), current_settings_.ApplicationVersion(),
      current_settings_.OrganizationName(), current_settings_.AppDataDirectory(),
      current_settings_.DatabaseDirectory(), current_settings_.EditorDistDirectory(),
      backend_base_url, backend_enabled_checkbox_->isChecked(), auth_authorization_url,
      auth_token_url, auth_client_id, auth_redirect_uri, auth_enabled_checkbox_->isChecked(),
      demo_collaboration_enabled_checkbox_->isChecked(), demo_collaboration_user_id,
      sync_enabled_checkbox_->isChecked(), font_size_spinbox_->value(),
      ai_features_enabled_checkbox_->isChecked(), ai_autocomplete_enabled_checkbox_->isChecked(),
      ai_inline_suggestions_enabled_checkbox_->isChecked());
}

}  // namespace cppwiki::gui
