#include "gui/page.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <set>
#include <string_view>

#include <QAbstractItemView>
#include <QIcon>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSize>
#include <QTimer>
#include <QVBoxLayout>
#include <QTreeView>
#include <QUrl>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineView>
#include <utility>

#include "auth/auth_session_manager.h"
#include "backend/backend_client.h"
#include "bridge/editor_bridge.h"
#include "core/constants.h"
#include "core/qt_string.h"
#include "gui/document_context_menu.h"
#include "gui/document_tree_item_delegate.h"
#include "gui/document_tree_model.h"
#include "gui/document_tree_view.h"
#include "app/editor_fallback.h"

namespace cppwiki {
namespace {

auto OptionalString(const QVariant& value) -> std::optional<std::string> {
  if (!value.isValid() || value.isNull()) {
    return std::nullopt;
  }

  const auto str = value.toString().trimmed();
  if (str.isEmpty()) {
    return std::nullopt;
  }

  return str.toStdString();
}

QVariant ValueFromFirstExistingKey(const QVariantMap& map, std::initializer_list<QString> keys) {
  for (const auto& key : keys) {
    if (map.contains(key)) {
      return map.value(key);
    }
  }

  return {};
}

auto OptionalParentId(const QVariantMap& document) -> std::optional<std::string> {
  // UI / bridge usually uses camelCase, while storage / JSON may use snake_case.
  return OptionalString(ValueFromFirstExistingKey(
      document, {QStringLiteral("parentId"), QStringLiteral("parent_id"),
                 QStringLiteral("parentDocumentId"), QStringLiteral("parent_document_id")}));
}

QString StringFromFirstExistingKey(const QVariantMap& map, std::initializer_list<QString> keys) {
  return ValueFromFirstExistingKey(map, keys).toString();
}

int IntFromFirstExistingKey(const QVariantMap& map, std::initializer_list<QString> keys,
                            int default_value = 0) {
  const auto value = ValueFromFirstExistingKey(map, keys);
  if (!value.isValid() || value.isNull()) {
    return default_value;
  }

  return value.toInt();
}

void VisitIndexes(const QAbstractItemModel* model, const QModelIndex& parent,
                  const std::function<void(const QModelIndex&)>& visitor) {
  if (model == nullptr) {
    return;
  }

  const int rows = model->rowCount(parent);
  for (int row = 0; row < rows; ++row) {
    const auto index = model->index(row, 0, parent);
    if (!index.isValid()) {
      continue;
    }
    visitor(index);
    VisitIndexes(model, index, visitor);
  }
}

}  // namespace

Page::Page(const AppContext& context, QWidget* parent)
    : QWidget(parent), context_(context) {
  BuildUi();
}

Page::~Page() = default;

QString Page::Title() const {
  return ToQString(constants::kDefaultPageTitle);
}

QWidget* Page::Widget() {
  return content_widget_;
}

QWidget* Page::SidebarWidget() const {
  return page_panel_;
}

QWidget* Page::ContentWidget() const {
  return content_widget_;
}

void Page::BuildUi() {
  // Create the QWebEngineView and QWebChannel.
  channel_ = new QWebChannel(this);
  editor_bridge_ = new bridge::QEditorBridge(this);
  channel_->registerObject(ToQString(constants::kDocumentsBridgeObjectName), editor_bridge_);

  // Set the document repository for the bridge
  editor_bridge_->SetRepository(context_.document_repository);

  editor_view_ = new QWebEngineView(this);
  editor_view_->page()->setWebChannel(channel_);
  InstallWebChannelScript();

  // Create tree view with custom model and delegate
  SetupTreeView();

  page_panel_ = new QWidget(this);
  page_panel_->setObjectName(QStringLiteral("pagePanel"));
  page_panel_->setAttribute(Qt::WA_StyledBackground, true);
  page_panel_->setFixedWidth(constants::kPageListInitialWidth);
  auto* page_panel_layout = new QVBoxLayout(page_panel_);
  page_panel_layout->setContentsMargins(12, 12, 12, 12);
  page_panel_layout->setSpacing(8);

  auto* app_title_label = new QLabel(ToQString(constants::kApplicationName), page_panel_);
  app_title_label->setObjectName(QStringLiteral("globalSidebarTitle"));
  page_panel_layout->addWidget(app_title_label, 0, Qt::AlignLeft);

  auto* controls_layout = new QHBoxLayout();
  controls_layout->setContentsMargins(0, 0, 0, 0);
  controls_layout->setSpacing(8);

  new_document_button_ = new QPushButton(QStringLiteral("New page"), page_panel_);
  new_document_button_->setObjectName(QStringLiteral("newDocumentButton"));
  new_document_button_->setIcon(QIcon::fromTheme(QStringLiteral("document-new")));
  new_document_button_->setIconSize(QSize(16, 16));
  new_document_button_->setCursor(Qt::PointingHandCursor);
  connect(new_document_button_, &QPushButton::clicked, this, [this]() {
    CreateNewDocument();
  });

  controls_layout->addWidget(new_document_button_);
  controls_layout->addStretch(1);

  page_panel_layout->addLayout(controls_layout);
  page_panel_layout->addWidget(page_tree_, 1);

  auto* sidebar_footer = new QFrame(page_panel_);
  sidebar_footer->setObjectName(QStringLiteral("sidebarFooter"));
  sidebar_footer->setFrameShape(QFrame::NoFrame);
  auto* sidebar_footer_layout = new QVBoxLayout(sidebar_footer);
  sidebar_footer_layout->setContentsMargins(0, 10, 0, 0);
  sidebar_footer_layout->setSpacing(10);

  settings_button_ = new QPushButton(QStringLiteral("Settings"), sidebar_footer);
  settings_button_->setObjectName(QStringLiteral("sidebarSettingsButton"));
  settings_button_->setIcon(QIcon::fromTheme(QStringLiteral("settings-configure")));
  settings_button_->setIconSize(QSize(16, 16));
  settings_button_->setCursor(Qt::PointingHandCursor);
  connect(settings_button_, &QPushButton::clicked, this, &Page::settingsRequested);
  sidebar_footer_layout->addWidget(settings_button_);

  auto* profile_card = new QFrame(sidebar_footer);
  profile_card->setObjectName(QStringLiteral("profilePlaceholderCard"));
  profile_card->setFrameShape(QFrame::NoFrame);
  auto* profile_card_layout = new QVBoxLayout(profile_card);
  profile_card_layout->setContentsMargins(12, 12, 12, 12);
  profile_card_layout->setSpacing(10);

  auto* profile_header_layout = new QHBoxLayout();
  profile_header_layout->setContentsMargins(0, 0, 0, 0);
  profile_header_layout->setSpacing(10);

  profile_avatar_label_ = new QLabel(QStringLiteral("A"), profile_card);
  profile_avatar_label_->setObjectName(QStringLiteral("profilePlaceholderAvatar"));
  profile_avatar_label_->setAlignment(Qt::AlignCenter);
  profile_avatar_label_->setFixedSize(32, 32);
  profile_header_layout->addWidget(profile_avatar_label_, 0, Qt::AlignTop);

  auto* profile_text_layout = new QVBoxLayout();
  profile_text_layout->setContentsMargins(0, 0, 0, 0);
  profile_text_layout->setSpacing(2);

  profile_name_label_ = new QLabel(QStringLiteral("Auth disabled"), profile_card);
  profile_name_label_->setObjectName(QStringLiteral("profilePlaceholderName"));
  profile_text_layout->addWidget(profile_name_label_);

  profile_hint_label_ =
      new QLabel(QStringLiteral("Enable auth in settings to use browser login."), profile_card);
  profile_hint_label_->setObjectName(QStringLiteral("profilePlaceholderHint"));
  profile_hint_label_->setWordWrap(true);
  profile_text_layout->addWidget(profile_hint_label_);
  profile_header_layout->addLayout(profile_text_layout, 1);

  profile_action_button_ = new QPushButton(QStringLiteral("Sign in"), profile_card);
  profile_action_button_->setObjectName(QStringLiteral("profileActionButton"));
  profile_action_button_->setCursor(Qt::PointingHandCursor);
  profile_action_button_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  profile_action_button_->setMinimumHeight(32);
  connect(profile_action_button_, &QPushButton::clicked, this, [this]() {
    if (context_.auth_session_manager == nullptr) {
      return;
    }

    if (context_.auth_session_manager->CanSignOut()) {
      context_.auth_session_manager->SignOut();
      return;
    }

    if (context_.auth_session_manager->CanStartSignIn()) {
      context_.auth_session_manager->StartSignIn();
    }
  });
  profile_card_layout->addLayout(profile_header_layout);
  profile_card_layout->addWidget(profile_action_button_);

  sidebar_footer_layout->addWidget(profile_card);
  page_panel_layout->addWidget(sidebar_footer, 0);

  content_widget_ = new QWidget(this);
  content_widget_->setObjectName(QStringLiteral("pageContentHost"));
  auto* content_layout = new QVBoxLayout(content_widget_);
  content_layout->setContentsMargins(0, 0, 0, 0);
  content_layout->setSpacing(0);
  content_layout->addWidget(editor_view_, 1);

  edit_inactivity_timer_ = new QTimer(this);
  edit_inactivity_timer_->setSingleShot(true);
  edit_inactivity_timer_->setInterval(constants::kEditModeInactivityTimeoutMinutes * 60 * 1000);
  connect(edit_inactivity_timer_, &QTimer::timeout, this, &Page::HandleEditInactivityTimeout);

  if (context_.auth_session_manager != nullptr) {
    connect(context_.auth_session_manager, &auth::AuthSessionManager::sessionChanged, this,
            [this]() {
              UpdateAuthCard();
              if (!selected_page_id_.isEmpty()) {
                OpenDocumentWithAccess(selected_page_id_);
              }
            });
  }
  if (context_.backend_client != nullptr) {
    connect(context_.backend_client, &backend::BackendClient::documentAccessInvalidated, this,
            [this](const QString& document_id, const QString& lock_owner,
                   const QString& status_text) {
              if (document_id != selected_page_id_ || editor_bridge_ == nullptr) {
                return;
              }

              editor_bridge_->SetCurrentDocumentAccess(false, lock_owner, status_text);
              ApplyDocumentAccessState(backend::DocumentAccessState{
                  .editable = false,
                  .local_only = false,
                  .lock_owner = lock_owner,
                  .status_text = status_text,
              });
            });
  }

  LoadEditor();
  PopulatePageList();
  UpdateAuthCard();
  UpdateEditModeControls();
}

void Page::SetupTreeView() {
  // Create tree model
  tree_model_ = std::make_unique<gui::DocumentTreeModel>(this);

  // Create tree view
  auto* document_tree_view = new gui::DocumentTreeView(this);
  page_tree_ = document_tree_view;
  page_tree_->setModel(tree_model_.get());
  page_tree_->setMinimumWidth(constants::kPageListMinimumWidth);
  page_tree_->setMaximumWidth(constants::kPageListMaximumWidth);
  page_tree_->setHeaderHidden(true);  // Hide header for cleaner look

  // Set up tree view styling for Qlementine look
  page_tree_->setUniformRowHeights(true);
  page_tree_->setAlternatingRowColors(false);  // Qlementine handles alternating colors
  page_tree_->setSelectionMode(QAbstractItemView::SingleSelection);
  page_tree_->setExpandsOnDoubleClick(true);
  page_tree_->setAnimated(true);

  // Qlementine-specific styling
  page_tree_->setFrameStyle(QFrame::NoFrame);  // Remove frame for cleaner look
  page_tree_->setRootIsDecorated(true);
  page_tree_->setIndentation(20);
  page_tree_->setItemsExpandable(true);
  page_tree_->setMouseTracking(true);
  page_tree_->viewport()->setMouseTracking(true);
  page_tree_->viewport()->setAttribute(Qt::WA_Hover, true);


  // Set custom delegate for icons and badges with Qlementine styling
  auto* delegate = new gui::DocumentTreeItemDelegate(page_tree_);
  page_tree_->setItemDelegate(delegate);

  // Configure tree view appearance
  page_tree_->setFocusPolicy(Qt::StrongFocus);
  page_tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  page_tree_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  page_tree_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  page_tree_->setContextMenuPolicy(Qt::CustomContextMenu);
  page_tree_->setDragEnabled(true);
  page_tree_->setAcceptDrops(true);
  page_tree_->setDropIndicatorShown(true);
  page_tree_->setDefaultDropAction(Qt::MoveAction);
  page_tree_->setDragDropMode(QAbstractItemView::InternalMove);
  page_tree_->setDragDropOverwriteMode(false);

  // Open documents from normal row clicks. Inline tree actions are handled by DocumentTreeView.
  connect(page_tree_, &QTreeView::clicked, this, [this](const QModelIndex& index) {
    if (index.isValid()) {
      OnTreePressed(index);
    }
  });
  connect(page_tree_, &QWidget::customContextMenuRequested, this,
          [this](const QPoint& position) { ShowContextMenu(position); });

  connect(document_tree_view, &gui::DocumentTreeView::addChildRequested, this,
          [this](const QModelIndex& parent_index) {
            CreateChildDocument(parent_index);
          });
  connect(tree_model_.get(), &gui::DocumentTreeModel::documentMoveRequested, this,
          [this](const QString& source_document_id, const QString& target_parent_id,
                 bool has_parent_id, int target_sort_order) {
            MoveDocumentToPlacement(source_document_id, target_parent_id, has_parent_id,
                                    target_sort_order);
          });
  connect(editor_bridge_, &bridge::QEditorBridge::saveStatusChanged, this,
          [this](const QString& page_id, bool success, const QString& message) {
            if (page_id == selected_page_id_ &&
                (message.contains(QStringLiteral("Saving"), Qt::CaseInsensitive) ||
                 (success && message.contains(QStringLiteral("Saved"), Qt::CaseInsensitive)))) {
              NoteEditActivity();
            }
            HandleDocumentSaved(page_id, success, message);
          });
  connect(editor_bridge_, &bridge::QEditorBridge::documentLoadFailed, this,
          [this](const QString&, const QString& message) {
            emit documentStatusChanged(QStringLiteral("Load error: %1").arg(message), true);
          });
}

void Page::LoadEditor() {
  const QString editor_index_path = context_.settings.EditorDistDirectory() + QStringLiteral("/index.html");
  const QFileInfo editor_index(editor_index_path);

  if (editor_index.exists() && editor_index.isFile()) {
    editor_view_->load(QUrl::fromLocalFile(editor_index.absoluteFilePath()));
    return;
  }

  editor_view_->setHtml(LoadEditorFallbackHtml(editor_index.absoluteFilePath()));
}

void Page::InstallWebChannelScript() {
  QFile script_file(QStringLiteral(":/qtwebchannel/qwebchannel.js"));
  if (!script_file.open(QIODevice::ReadOnly)) {
    return;
  }

  QWebEngineScript script;
  script.setName(QStringLiteral("qwebchannel"));
  script.setSourceCode(QString::fromUtf8(script_file.readAll()));
  script.setInjectionPoint(QWebEngineScript::DocumentCreation);
  script.setWorldId(QWebEngineScript::MainWorld);
  script.setRunsOnSubFrames(false);

  editor_view_->page()->scripts().insert(script);
}

void Page::PopulatePageList() {
  const auto expanded_ids = CaptureExpandedDocumentIds();
  const auto response = editor_bridge_->listDocuments();
  if (!response.value(QStringLiteral("ok")).toBool()) {
    const auto error = response.value(QStringLiteral("error")).toMap();
    spdlog::error("Failed to list documents for Qt navigation: {}",
                  error.value(QStringLiteral("message")).toString().toStdString());
    return;
  }

  const auto documents = response.value(QStringLiteral("result")).toList();

  std::vector<storage::DocumentSummary> summaries;
  summaries.reserve(static_cast<size_t>(documents.size()));

  for (const auto& document_value : documents) {
    auto summary = SummaryFromVariantMap(document_value.toMap());

    spdlog::debug("Document summary: id={}, title={}, parent_id={}, sort_order={}",
                  summary.id, summary.title, summary.parent_id.value_or("<root>"),
                  summary.sort_order);

    summaries.push_back(std::move(summary));
  }

  tree_model_->setDocuments(summaries);
  RestoreExpandedDocumentIds(expanded_ids);
}

void Page::OnTreePressed(const QModelIndex& index) {
  const auto doc_id = tree_model_->documentId(index);
  if (!doc_id) {
    return;
  }

  selected_page_id_ = QString::fromStdString(*doc_id);
  OpenDocumentWithAccess(selected_page_id_);
}

void Page::CreateNewDocument() {
  const auto response = editor_bridge_->createDocument();
  HandleCreatedDocument(response);
}

void Page::CreateChildDocument(const QModelIndex& parent_index) {
  const auto parent_id = MapToParentDocumentId(parent_index);
  if (!parent_id) {
    spdlog::warn("Cannot create child document: invalid parent index");
    return;
  }

  const auto response = editor_bridge_->createChildDocument(QString::fromStdString(*parent_id));
  HandleCreatedDocument(response);
}

void Page::RenameDocument(const QModelIndex& index) {
  const auto doc_id = tree_model_->documentId(index);
  if (!doc_id) {
    return;
  }

  bool accepted = false;
  const auto current_title = index.data(Qt::DisplayRole).toString();
  const auto new_title = QInputDialog::getText(page_tree_, QStringLiteral("Rename title"),
                                               QStringLiteral("Title:"), QLineEdit::Normal,
                                               current_title, &accepted)
                             .trimmed();
  if (!accepted || new_title.isEmpty() || new_title == current_title) {
    return;
  }

  const auto response = editor_bridge_->renameDocument(QString::fromStdString(*doc_id), new_title);
  if (!response.value(QStringLiteral("ok")).toBool()) {
    const auto error = response.value(QStringLiteral("error")).toMap();
    spdlog::error("Failed to rename document: {}",
                  error.value(QStringLiteral("message")).toString().toStdString());
    return;
  }

  PopulatePageList();
  SelectDocumentById(QString::fromStdString(*doc_id));
}

void Page::HandleCreatedDocument(const QVariantMap& response) {
  if (!response.value(QStringLiteral("ok")).toBool()) {
    const auto error = response.value(QStringLiteral("error")).toMap();
    spdlog::error("Failed to create document: {}",
                  error.value(QStringLiteral("message")).toString().toStdString());
    return;
  }

  const auto created = response.value(QStringLiteral("result")).toMap();
  const auto page_id = created.value(QStringLiteral("id")).toString();
  if (page_id.isEmpty()) {
    spdlog::error("Created document response does not contain an id");
    return;
  }

  tree_model_->appendDocument(SummaryFromVariantMap(created));
  selected_page_id_ = page_id;
  ExpandAncestors(page_id);
  SelectDocumentById(page_id);
  OpenDocumentWithAccess(page_id);
}

void Page::SelectDocumentById(const QString& page_id) {
  if (!tree_model_ || page_id.isEmpty()) {
    return;
  }

  const auto index = tree_model_->indexForDocumentId(page_id.toStdString());
  if (index.isValid()) {
    ExpandAncestors(page_id);
    page_tree_->setCurrentIndex(index);
    page_tree_->scrollTo(index, QAbstractItemView::PositionAtCenter);
  }
}

void Page::DeleteDocument(const QModelIndex& index) {
  const auto doc_id = tree_model_->documentId(index);
  if (!doc_id) {
    return;
  }

  const auto response = editor_bridge_->deleteDocument(QString::fromStdString(*doc_id));
  if (!response.value(QStringLiteral("ok")).toBool()) {
    const auto error = response.value(QStringLiteral("error")).toMap();
    spdlog::error("Failed to delete document: {}",
                  error.value(QStringLiteral("message")).toString().toStdString());
    return;
  }

  PopulatePageList();

  const auto summaries = FetchDocumentSummaries();
  if (summaries.empty()) {
    selected_page_id_.clear();
    if (context_.backend_client != nullptr) {
      context_.backend_client->CloseDocumentSession();
    }
    editor_bridge_->ClearCurrentDocumentSelection();
    return;
  }

  selected_page_id_ = QString::fromStdString(summaries.front().id);
  SelectDocumentById(selected_page_id_);
  OpenDocumentWithAccess(selected_page_id_);
}

void Page::OpenDocumentWithAccess(const QString& page_id) {
  if (editor_bridge_ == nullptr || page_id.isEmpty()) {
    return;
  }

  if (context_.backend_client == nullptr) {
    StopEditInactivityTimer();
    editor_bridge_->SetPendingDocumentAccess(true, QString{},
                                             QStringLiteral("Document: local-only editing"));
    emit documentStatusChanged(QStringLiteral("Document: local-only editing"), false);
    emit collaborationStatusChanged(QStringLiteral("Collab: local only"),
                                    QStringLiteral("Backend lock flow is not active."), false);
    editor_bridge_->RequestOpenDocument(page_id);
    return;
  }

  context_.backend_client->OpenDocumentViewSession(
      page_id, [this, page_id](backend::DocumentAccessState access_state) {
        if (page_id != selected_page_id_ || editor_bridge_ == nullptr) {
          return;
        }

        editor_bridge_->SetPendingDocumentAccess(access_state.editable, access_state.lock_owner,
                                                 access_state.status_text);
        ApplyDocumentAccessState(access_state);
        editor_bridge_->RequestOpenDocument(page_id);
      });
}

void Page::EnterEditMode() {
  if (selected_page_id_.isEmpty() || context_.backend_client == nullptr || editor_bridge_ == nullptr) {
    return;
  }

  context_.backend_client->EnterDocumentEditSession(
      selected_page_id_, [this](backend::DocumentAccessState access_state) {
        if (editor_bridge_ == nullptr) {
          return;
        }
        editor_bridge_->SetCurrentDocumentAccess(access_state.editable, access_state.lock_owner,
                                                 access_state.status_text);
        ApplyDocumentAccessState(access_state);
        if (access_state.editable) {
          NoteEditActivity();
        }
      });
}

void Page::ExitEditMode(bool due_to_inactivity) {
  if (selected_page_id_.isEmpty() || context_.backend_client == nullptr || editor_bridge_ == nullptr) {
    return;
  }

  StopEditInactivityTimer();
  pending_inactivity_exit_notice_ = due_to_inactivity;
  context_.backend_client->ExitDocumentEditSession(
      selected_page_id_, [this](backend::DocumentAccessState access_state) {
        if (editor_bridge_ == nullptr) {
          return;
        }
        editor_bridge_->SetCurrentDocumentAccess(access_state.editable, access_state.lock_owner,
                                                 access_state.status_text);
        ApplyDocumentAccessState(access_state);
        if (pending_inactivity_exit_notice_ && !access_state.editable) {
          emit documentStatusChanged(
              QStringLiteral("Edit mode ended after %1 minutes of inactivity.")
                  .arg(constants::kEditModeInactivityTimeoutMinutes),
              false);
          emit collaborationStatusChanged(
              QStringLiteral("Collab: viewing"),
              QStringLiteral("Edit mode timed out after %1 minutes without changes.")
                  .arg(constants::kEditModeInactivityTimeoutMinutes),
              false);
        }
        pending_inactivity_exit_notice_ = false;
      });
}

void Page::ToggleEditMode() {
  switch (current_document_editable_) {
    case true:
      ExitEditMode();
      break;

    case false:
      EnterEditMode();
      break;
  }
}

void Page::ApplyDocumentAccessState(const backend::DocumentAccessState& access_state) {
  current_document_editable_ = access_state.editable;
  current_document_local_only_ = access_state.local_only;
  if (current_document_editable_ && !current_document_local_only_) {
    StartEditInactivityTimer();
  } else {
    StopEditInactivityTimer();
  }
  UpdateEditModeControls();

  emit documentStatusChanged(access_state.status_text, false);
  if (access_state.local_only) {
    emit collaborationStatusChanged(QStringLiteral("Collab: local only"),
                                    QStringLiteral("Backend lock flow is not active."), false);
    return;
  }

  if (access_state.editable) {
    emit collaborationStatusChanged(
        QStringLiteral("Collab: editing"),
        access_state.lock_owner.trimmed().isEmpty()
            ? QStringLiteral("Backend lock acquired.")
            : QStringLiteral("Owner: %1").arg(access_state.lock_owner),
        false);
    return;
  }

  if (access_state.status_text.contains(QStringLiteral("lock lost"), Qt::CaseInsensitive) ||
      access_state.status_text.contains(QStringLiteral("heartbeat failed"), Qt::CaseInsensitive)) {
    emit collaborationStatusChanged(
        QStringLiteral("Collab: lock lost"),
        QStringLiteral("Editing access was lost. Re-enter edit mode when the backend is reachable."),
        true);
    return;
  }

  if (access_state.status_text.contains(QStringLiteral("unauthorized"), Qt::CaseInsensitive) ||
      access_state.status_text.contains(QStringLiteral("expired"), Qt::CaseInsensitive)) {
    emit collaborationStatusChanged(
        QStringLiteral("Collab: session expired"),
        QStringLiteral("Authentication expired. Sign in again to resume collaborative editing."),
        true);
    return;
  }

  if (access_state.status_text.contains(QStringLiteral("failed"), Qt::CaseInsensitive) ||
      access_state.status_text.contains(QStringLiteral("rejected"), Qt::CaseInsensitive)) {
    emit collaborationStatusChanged(QStringLiteral("Collab: unavailable"), access_state.status_text,
                                    true);
    return;
  }

  if (access_state.status_text.contains(QStringLiteral("locked by"), Qt::CaseInsensitive)) {
    emit collaborationStatusChanged(
        QStringLiteral("Collab: read-only"),
        access_state.lock_owner.trimmed().isEmpty()
            ? access_state.status_text
            : QStringLiteral("Locked by %1").arg(access_state.lock_owner),
        true);
    return;
  }

  emit collaborationStatusChanged(QStringLiteral("Collab: viewing"),
                                  QStringLiteral("Enter edit mode to acquire the lock."), false);
}

void Page::UpdateEditModeControls() {
  if (selected_page_id_.isEmpty()) {
    emit editModeStateChanged(QStringLiteral("No document selected"), false, false);
    return;
  }

  if (current_document_local_only_) {
    emit editModeStateChanged(QStringLiteral("Local editing"), false, false);
    return;
  }

  if (current_document_editable_) {
    emit editModeStateChanged(QStringLiteral("Edit mode"), true, true);
  } else {
    emit editModeStateChanged(QStringLiteral("View mode"), false, true);
  }
}

void Page::StartEditInactivityTimer() {
  if (edit_inactivity_timer_ == nullptr || current_document_local_only_ || !current_document_editable_) {
    return;
  }

  edit_inactivity_timer_->start();
}

void Page::StopEditInactivityTimer() {
  if (edit_inactivity_timer_ == nullptr) {
    return;
  }

  edit_inactivity_timer_->stop();
}

void Page::NoteEditActivity() {
  StartEditInactivityTimer();
}

void Page::HandleEditInactivityTimeout() {
  if (!current_document_editable_ || current_document_local_only_) {
    return;
  }

  ExitEditMode(true);
}

void Page::MoveDocument(const QModelIndex& index, int delta) {
  const auto doc_id = tree_model_->documentId(index);
  if (!doc_id || delta == 0) {
    return;
  }

  auto summaries = FetchDocumentSummaries();
  const auto target_it = std::find_if(summaries.begin(), summaries.end(), [&](const auto& summary) {
    return summary.id == *doc_id;
  });
  if (target_it == summaries.end()) {
    return;
  }

  const auto same_parent = [&](const storage::DocumentSummary& summary) {
    return summary.parent_id == target_it->parent_id;
  };

  std::vector<std::reference_wrapper<storage::DocumentSummary>> siblings;
  for (auto& summary : summaries) {
    if (same_parent(summary)) {
      siblings.push_back(summary);
    }
  }

  std::ranges::sort(siblings, [](const auto& lhs, const auto& rhs) {
    if (lhs.get().sort_order != rhs.get().sort_order) {
      return lhs.get().sort_order < rhs.get().sort_order;
    }
    return lhs.get().title < rhs.get().title;
  });

  auto sibling_it = std::find_if(siblings.begin(), siblings.end(), [&](const auto& summary) {
    return summary.get().id == *doc_id;
  });
  if (sibling_it == siblings.end()) {
    return;
  }

  const auto position = static_cast<std::ptrdiff_t>(std::distance(siblings.begin(), sibling_it));
  const auto new_position = position + delta;
  if (new_position < 0 ||
      new_position >= static_cast<std::ptrdiff_t>(siblings.size())) {
    return;
  }

  std::iter_swap(siblings.begin() + position, siblings.begin() + new_position);

  for (std::size_t i = 0; i < siblings.size(); ++i) {
    auto& summary = siblings[i].get();
    const auto response = editor_bridge_->updateDocumentPlacement(
        QString::fromStdString(summary.id),
        summary.parent_id ? QString::fromStdString(*summary.parent_id) : QString{},
        summary.parent_id.has_value(),
        static_cast<int>(i));
    if (!response.value(QStringLiteral("ok")).toBool()) {
      const auto error = response.value(QStringLiteral("error")).toMap();
      spdlog::error("Failed to move document: {}",
                    error.value(QStringLiteral("message")).toString().toStdString());
      return;
    }
  }

  PopulatePageList();
  SelectDocumentById(QString::fromStdString(*doc_id));
}

void Page::MoveDocumentToPlacement(const QString& source_document_id, const QString& target_parent_id,
                                   bool has_parent_id, int target_sort_order) {
  if (!tree_model_ || source_document_id.isEmpty()) {
    return;
  }

  if (has_parent_id && target_parent_id == source_document_id) {
    return;
  }

  auto summaries = FetchDocumentSummaries();
  auto source_it = std::find_if(summaries.begin(), summaries.end(), [&](const auto& summary) {
    return summary.id == source_document_id.toStdString();
  });
  if (source_it == summaries.end()) {
    return;
  }

  const auto source_id_std = source_document_id.toStdString();
  if (has_parent_id) {
    std::optional<std::string> cursor = target_parent_id.toStdString();
    while (cursor.has_value()) {
      if (*cursor == source_id_std) {
        return;
      }
      const auto parent_it = std::find_if(summaries.begin(), summaries.end(), [&](const auto& summary) {
        return summary.id == *cursor;
      });
      if (parent_it == summaries.end()) {
        break;
      }
      cursor = parent_it->parent_id;
    }
  }

  const auto old_parent = source_it->parent_id;
  const auto new_parent = has_parent_id ? std::make_optional(target_parent_id.toStdString())
                                        : std::nullopt;

  auto build_group = [&](const std::optional<std::string>& parent_id,
                         std::string_view skip_id) {
    std::vector<std::reference_wrapper<storage::DocumentSummary>> group;
    for (auto& summary : summaries) {
      if (summary.parent_id == parent_id && summary.id != skip_id) {
        group.push_back(summary);
      }
    }
    std::ranges::sort(group, [](const auto& lhs, const auto& rhs) {
      if (lhs.get().sort_order != rhs.get().sort_order) {
        return lhs.get().sort_order < rhs.get().sort_order;
      }
      return lhs.get().title < rhs.get().title;
    });
    return group;
  };

  auto persist_group = [this](std::vector<std::reference_wrapper<storage::DocumentSummary>>& group,
                              const std::optional<std::string>& parent_id) -> bool {
    for (std::size_t i = 0; i < group.size(); ++i) {
      auto& summary = group[i].get();
      const auto response = editor_bridge_->updateDocumentPlacement(
          QString::fromStdString(summary.id),
          parent_id ? QString::fromStdString(*parent_id) : QString{},
          parent_id.has_value(),
          static_cast<int>(i));
      if (!response.value(QStringLiteral("ok")).toBool()) {
        const auto error = response.value(QStringLiteral("error")).toMap();
        spdlog::error("Failed to move document: {}",
                      error.value(QStringLiteral("message")).toString().toStdString());
        return false;
      }
    }
    return true;
  };

  if (old_parent == new_parent) {
    auto group = build_group(old_parent, source_id_std);
    auto source_ref = std::ref(*source_it);
    const auto insert_row = std::clamp(target_sort_order, 0, static_cast<int>(group.size()));
    group.insert(group.begin() + insert_row, source_ref);

    if (!persist_group(group, old_parent)) {
      return;
    }
  } else {
    auto old_group = build_group(old_parent, source_id_std);
    auto new_group = build_group(new_parent, source_id_std);
    auto source_ref = std::ref(*source_it);
    const auto insert_row = std::clamp(target_sort_order, 0, static_cast<int>(new_group.size()));
    new_group.insert(new_group.begin() + insert_row, source_ref);

    if (!persist_group(old_group, old_parent)) {
      return;
    }
    if (!persist_group(new_group, new_parent)) {
      return;
    }
  }

  PopulatePageList();
  SelectDocumentById(source_document_id);
}

void Page::ShowContextMenu(const QPoint& position) {
  const auto index = page_tree_->indexAt(position);
  const auto doc_id = tree_model_->documentId(index);
  if (!doc_id) {
    return;
  }

  page_tree_->setCurrentIndex(index);
  spdlog::info("Context menu requested");

  const auto summaries = FetchDocumentSummaries();
  const auto current_it = std::find_if(summaries.begin(), summaries.end(), [&](const auto& summary) {
    return summary.id == *doc_id;
  });

  auto same_parent_siblings = std::vector<const storage::DocumentSummary*>{};
  if (current_it != summaries.end()) {
    for (const auto& summary : summaries) {
      if (summary.parent_id == current_it->parent_id) {
        same_parent_siblings.push_back(&summary);
      }
    }
    std::ranges::sort(same_parent_siblings, [](const auto* lhs, const auto* rhs) {
      if (lhs->sort_order != rhs->sort_order) {
        return lhs->sort_order < rhs->sort_order;
      }
      return lhs->title < rhs->title;
    });
  }

  const auto current_sibling_it = std::find_if(same_parent_siblings.begin(), same_parent_siblings.end(),
                                               [&](const auto* summary) {
                                                 return summary->id == *doc_id;
                                               });
  const bool can_move_up =
      current_sibling_it != same_parent_siblings.end() && current_sibling_it != same_parent_siblings.begin();
  const bool can_move_down =
      current_sibling_it != same_parent_siblings.end() &&
      std::next(current_sibling_it) != same_parent_siblings.end();

  auto* menu = new gui::DocumentContextMenu(
      {.can_move_up = can_move_up, .can_move_down = can_move_down}, page_tree_);
  connect(menu, &gui::DocumentContextMenu::actionRequested, this,
          [this, index](gui::DocumentContextMenu::Action action) {
            switch (action) {
              case gui::DocumentContextMenu::Action::kAddChildPage:
                spdlog::info("Context menu: add child page");
                CreateChildDocument(index);
                break;
              case gui::DocumentContextMenu::Action::kRenameTitle:
                spdlog::info("Context menu: rename title");
                RenameDocument(index);
                break;
              case gui::DocumentContextMenu::Action::kMoveUp:
                spdlog::info("Context menu: move up");
                MoveDocument(index, -1);
                break;
              case gui::DocumentContextMenu::Action::kMoveDown:
                spdlog::info("Context menu: move down");
                MoveDocument(index, 1);
                break;
              case gui::DocumentContextMenu::Action::kDeletePage:
                spdlog::info("Context menu: delete page");
                DeleteDocument(index);
                break;
            }
          });
  menu->ShowAt(page_tree_->mapToGlobal(position));
}

void Page::HandleDocumentSaved(const QString& page_id, bool success, const QString& message) {
  if (selected_page_id_.isEmpty() || page_id != selected_page_id_) {
    return;
  }

  const auto is_read_only_status =
      message.contains(QStringLiteral("read-only"), Qt::CaseInsensitive) ||
      message.contains(QStringLiteral("viewing"), Qt::CaseInsensitive);
  if (!success && is_read_only_status) {
    emit documentStatusChanged(message, false);
    return;
  }

  emit documentStatusChanged(success ? message : QStringLiteral("Save error: %1").arg(message),
                             !success);

  if (!success || message != QStringLiteral("Saved")) {
    return;
  }

  PopulatePageList();
  SelectDocumentById(selected_page_id_);
}

std::set<std::string> Page::CaptureExpandedDocumentIds() const {
  std::set<std::string> expanded_ids;
  if (page_tree_ == nullptr || tree_model_ == nullptr) {
    return expanded_ids;
  }

  VisitIndexes(tree_model_.get(), QModelIndex{}, [&](const QModelIndex& index) {
    if (!page_tree_->isExpanded(index)) {
      return;
    }
    if (const auto doc_id = tree_model_->documentId(index); doc_id.has_value()) {
      expanded_ids.insert(*doc_id);
    }
  });
  return expanded_ids;
}

void Page::RestoreExpandedDocumentIds(const std::set<std::string>& expanded_ids) {
  if (page_tree_ == nullptr || tree_model_ == nullptr) {
    return;
  }

  for (const auto& doc_id : expanded_ids) {
    const auto index = tree_model_->indexForDocumentId(doc_id);
    if (index.isValid()) {
      page_tree_->setExpanded(index, true);
    }
  }
}

void Page::ExpandAncestors(const QString& page_id) {
  if (page_tree_ == nullptr || tree_model_ == nullptr || page_id.isEmpty()) {
    return;
  }

  auto index = tree_model_->indexForDocumentId(page_id.toStdString());
  while (index.isValid()) {
    const auto parent = index.parent();
    if (!parent.isValid()) {
      break;
    }
    page_tree_->setExpanded(parent, true);
    index = parent;
  }
}

std::vector<storage::DocumentSummary> Page::FetchDocumentSummaries() const {
  std::vector<storage::DocumentSummary> summaries;
  const auto response = editor_bridge_->listDocuments();
  if (!response.value(QStringLiteral("ok")).toBool()) {
    return summaries;
  }

  const auto documents = response.value(QStringLiteral("result")).toList();
  summaries.reserve(static_cast<std::size_t>(documents.size()));
  for (const auto& document_value : documents) {
    summaries.push_back(SummaryFromVariantMap(document_value.toMap()));
  }
  return summaries;
}

std::optional<std::string> Page::MapToParentDocumentId(const QModelIndex& index) const {
  if (!index.isValid() || !tree_model_) {
    return std::nullopt;
  }

  // The hover "+" button lives on the document row itself.
  return tree_model_->documentId(index);
}

storage::DocumentSummary Page::SummaryFromVariantMap(const QVariantMap& document) const {
  storage::DocumentSummary summary;
  summary.id = StringFromFirstExistingKey(document, {QStringLiteral("id")}).toStdString();
  summary.title = StringFromFirstExistingKey(document, {QStringLiteral("title")}).toStdString();
  summary.parent_id = OptionalParentId(document);
  summary.sort_order = IntFromFirstExistingKey(
      document, {QStringLiteral("sortOrder"), QStringLiteral("sort_order")});
  summary.created_at = StringFromFirstExistingKey(
      document, {QStringLiteral("createdAt"), QStringLiteral("created_at")}).toStdString();
  summary.updated_at = StringFromFirstExistingKey(
      document, {QStringLiteral("updatedAt"), QStringLiteral("updated_at")}).toStdString();
  return summary;
}

void Page::UpdateAuthCard() {
  if (profile_avatar_label_ == nullptr || profile_name_label_ == nullptr ||
      profile_hint_label_ == nullptr || profile_action_button_ == nullptr) {
    return;
  }

  if (context_.auth_session_manager == nullptr) {
    profile_avatar_label_->setText(QStringLiteral("A"));
    profile_name_label_->setText(QStringLiteral("Auth unavailable"));
    profile_hint_label_->setText(QStringLiteral("Auth session manager is not available."));
    profile_action_button_->setText(QStringLiteral("Sign in"));
    profile_action_button_->setEnabled(false);
    return;
  }

  const auto* auth = context_.auth_session_manager;
  profile_avatar_label_->setText(auth->ProfileAvatarText());
  profile_name_label_->setText(auth->ProfileName());
  profile_hint_label_->setText(QStringLiteral("%1\n%2").arg(auth->ProfileHint(), auth->Subtitle()));
  profile_action_button_->setText(auth->ActionLabel());
  profile_action_button_->setEnabled(auth->CanStartSignIn() || auth->CanSignOut());
}

}  // namespace cppwiki
