#include "gui/page.h"

#include <spdlog/spdlog.h>

#include <QAbstractItemView>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QSize>
#include <QTimer>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineView>
#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <set>
#include <string_view>
#include <utility>

#include "app/editor_fallback.h"
#include "auth/ai_api_key_store.h"
#include "auth/auth_session_manager.h"
#include "backend/backend_client.h"
#include "bridge/editor_bridge.h"
#include "core/constants.h"
#include "core/qt_string.h"
#include "gui/document_context_menu.h"
#include "gui/document_tree_item_delegate.h"
#include "gui/document_tree_model.h"
#include "gui/document_tree_view.h"
#include "gui/page_helpers.h"
#include "sync/sync_service.h"

namespace cppwiki {
namespace {

using namespace gui::page_helpers;

}  // namespace

Page::Page(const AppContext& context, QWidget* parent) : QWidget(parent), context_(context) {
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
  editor_bridge_->SetSyncStateProvider(context_.document_sync_service);

  // BlockNote AI wiring (ADR-010/ADR-012): the bridge decides server-mediated
  // vs. local-key fallback based on whether a backend is configured; the
  // settings toggles gate whether the JS side even shows AI UI at all.
  editor_bridge_->SetAiFeatureFlags(context_.settings.AiFeaturesEnabled(),
                                    context_.settings.AiAutocompleteEnabled(),
                                    context_.settings.AiInlineSuggestionsEnabled());
  editor_bridge_->SetAiTransportConfig(context_.settings.BackendEnabled(),
                                      context_.settings.BackendBaseUrl(), QString{});
  auto* ai_api_key_store =
      new auth::AiApiKeyStore(ToQString(constants::kApplicationName), editor_bridge_);
  editor_bridge_->SetAiApiKeyStore(ai_api_key_store);
  if (context_.auth_session_manager != nullptr) {
    connect(context_.auth_session_manager, &auth::AuthSessionManager::accessTokenChanged,
            editor_bridge_, [this](const QString& access_token) {
              editor_bridge_->SetAiTransportConfig(context_.settings.BackendEnabled(),
                                                  context_.settings.BackendBaseUrl(), access_token);
            });
  }

  editor_view_ = new QWebEngineView(this);
  editor_view_->page()->setWebChannel(channel_);
  InstallWebChannelScript();

  page_panel_ = new QWidget(this);
  page_panel_->setObjectName(QStringLiteral("pagePanel"));
  page_panel_->setAttribute(Qt::WA_StyledBackground, true);
  page_panel_->setFixedWidth(constants::kPageListInitialWidth);
  auto* page_panel_layout = new QVBoxLayout(page_panel_);
  page_panel_layout->setContentsMargins(12, 12, 12, 12);
  page_panel_layout->setSpacing(8);

  auto* app_title_label = new QLabel(ToQString(constants::kApplicationName), page_panel_);
  app_title_label->setObjectName(QStringLiteral("globalSidebarTitle"));
  auto* title_row_layout = new QHBoxLayout();
  title_row_layout->setContentsMargins(0, 0, 0, 0);
  title_row_layout->setSpacing(8);
  title_row_layout->addWidget(app_title_label, 1, Qt::AlignLeft);

  page_panel_layout->addLayout(title_row_layout);

  workspace_tree_model_ = std::make_unique<gui::DocumentTreeModel>(this);
  workspace_tree_view_ = new gui::DocumentTreeView(page_panel_);
  workspace_tree_view_->setObjectName(QStringLiteral("workspaceTreeView"));
  workspace_tree_view_->setModel(workspace_tree_model_.get());
  workspace_tree_view_->setMinimumWidth(constants::kPageListMinimumWidth);
  workspace_tree_view_->setMaximumWidth(constants::kPageListMaximumWidth);
  workspace_tree_view_->setHeaderHidden(true);
  workspace_tree_view_->setUniformRowHeights(true);
  workspace_tree_view_->setAlternatingRowColors(false);
  workspace_tree_view_->setExpandsOnDoubleClick(true);
  workspace_tree_view_->setAnimated(true);
  workspace_tree_view_->setFrameStyle(QFrame::NoFrame);
  workspace_tree_view_->setRootIsDecorated(true);
  workspace_tree_view_->setIndentation(20);
  workspace_tree_view_->setItemsExpandable(true);
  workspace_tree_view_->setMouseTracking(true);
  workspace_tree_view_->viewport()->setMouseTracking(true);
  workspace_tree_view_->viewport()->setAttribute(Qt::WA_Hover, true);
  auto* delegate = new gui::DocumentTreeItemDelegate(workspace_tree_view_);
  workspace_tree_view_->setItemDelegate(delegate);
  workspace_tree_view_->setFocusPolicy(Qt::StrongFocus);
  workspace_tree_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  workspace_tree_view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  workspace_tree_view_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  workspace_tree_view_->setContextMenuPolicy(Qt::CustomContextMenu);
  workspace_tree_view_->setDragEnabled(true);
  workspace_tree_view_->setAcceptDrops(true);
  workspace_tree_view_->setDropIndicatorShown(true);
  workspace_tree_view_->setDefaultDropAction(Qt::MoveAction);
  workspace_tree_view_->setDragDropMode(QAbstractItemView::InternalMove);
  workspace_tree_view_->setDragDropOverwriteMode(false);
  page_panel_layout->addWidget(workspace_tree_view_, 1);

  connect(workspace_tree_view_, &QTreeView::clicked, this, [this](const QModelIndex& index) {
    if (index.isValid()) {
      OnTreePressed(index);
    }
  });
  connect(workspace_tree_view_, &QWidget::customContextMenuRequested, this,
          [this](const QPoint& position) { ShowContextMenu(position); });
  connect(workspace_tree_view_, &gui::DocumentTreeView::addChildRequested, this,
          [this](const QModelIndex& parent_index) { CreateChildDocument(parent_index); });
  connect(workspace_tree_model_.get(), &gui::DocumentTreeModel::documentMoveRequested, this,
          [this](const QString& source_document_id, const QString& target_parent_id,
                 bool has_parent_id, int target_sort_order, const QString& workspace_id) {
            MoveDocumentToPlacement(source_document_id, target_parent_id, has_parent_id,
                                    target_sort_order, workspace_id);
          });

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
              RefreshSelectedDocumentAccess();
            });
  }
  if (context_.backend_client != nullptr) {
    connect(context_.backend_client, &backend::BackendClient::syncBootstrapChanged, this, [this]() {
      ApplyBridgeSessionContext();
      PopulatePageList();
      RefreshSelectedDocumentAccess();
    });
    connect(
        context_.backend_client, &backend::BackendClient::documentAccessInvalidated, this,
        [this](const QString& document_id, const QString& lock_owner, const QString& status_text) {
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
  if (context_.document_sync_service != nullptr) {
    connect(context_.document_sync_service, &sync::SyncService::snapshotChanged, this,
            [this](const sync::DocumentSyncSnapshot&) {
              ApplyBridgeSessionContext();
              RefreshPageListIfChanged();
              RefreshSelectedDocumentAccess();
            });
  }
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
  connect(editor_bridge_, &bridge::QEditorBridge::documentLoaded, this,
          [this](const QVariantMap& document) {
            ApplyConflictStateForDocument(document.value(QStringLiteral("id")).toString());
          });

  ApplyBridgeSessionContext();
  LoadEditor();
  PopulatePageList();
  UpdateAuthCard();
  UpdateEditModeControls();
}

void Page::ApplyBridgeSessionContext() {
  if (editor_bridge_ == nullptr) {
    return;
  }

  editor_bridge_->SetCurrentAuthorId(EffectiveAuthorId(context_));
  available_workspace_ids_ = EffectiveWorkspaceIds(context_);
  const auto preferred_workspace_id = PreferredWorkspaceId(context_, available_workspace_ids_);
  const auto selected_workspace_id = available_workspace_ids_.contains(current_workspace_id_)
                                         ? current_workspace_id_
                                         : preferred_workspace_id;
  ActivateWorkspace(selected_workspace_id);
  RebuildWorkspaceTree();
}

void Page::ActivateWorkspace(const QString& workspace_id) {
  const auto normalized_workspace_id =
      workspace_id.trimmed().isEmpty() ? QStringLiteral("default") : workspace_id.trimmed();
  if (current_workspace_id_ == normalized_workspace_id) {
    return;
  }

  current_workspace_id_ = normalized_workspace_id;
  if (editor_bridge_ != nullptr) {
    editor_bridge_->SetCurrentWorkspaceId(current_workspace_id_);
  }
  selected_page_id_.clear();
  current_document_editable_ = false;
  current_document_local_only_ = true;
  pending_inactivity_exit_notice_ = false;
  ApplyDocumentAccessState(backend::DocumentAccessState{
      .editable = false,
      .local_only = true,
      .lock_owner = {},
      .status_text = QStringLiteral("Document: select a page in %1").arg(current_workspace_id_),
  });
}

void Page::RefreshWorkspaceHydrationState() {
  if (workspace_tree_model_ == nullptr || context_.document_sync_service == nullptr) {
    return;
  }

  const auto& snapshot = context_.document_sync_service->Snapshot();
  if (snapshot.workspace_ids.isEmpty()) {
    return;
  }

  for (const auto& workspace_id : snapshot.workspace_ids) {
    const auto state = snapshot.workspace_hydration.value(
        workspace_id, sync::WorkspaceHydrationState::kNotStarted);
    QString decoration;
    switch (state) {
      case sync::WorkspaceHydrationState::kNotStarted:
        decoration = QStringLiteral(" — not downloaded");
        break;
      case sync::WorkspaceHydrationState::kInProgress:
        decoration = QStringLiteral(" — syncing...");
        break;
      case sync::WorkspaceHydrationState::kFailed:
        decoration = QStringLiteral(" — sync failed");
        break;
      case sync::WorkspaceHydrationState::kMaterialized:
        break;
    }
    workspace_tree_model_->setWorkspaceDecoration(workspace_id, decoration);
  }
}

void Page::RebuildWorkspaceTree() {
  if (workspace_tree_model_ == nullptr) {
    return;
  }

  if (workspace_tree_view_ != nullptr) {
    workspace_tree_view_->setUpdatesEnabled(false);
  }
  PopulatePageList();
  RefreshWorkspaceHydrationState();
  ExpandWorkspace(current_workspace_id_);
  if (workspace_tree_view_ != nullptr) {
    workspace_tree_view_->setUpdatesEnabled(true);
    workspace_tree_view_->viewport()->update();
  }
}

void Page::LoadEditor() {
  const QString editor_index_path =
      context_.settings.EditorDistDirectory() + QStringLiteral("/index.html");
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
  const auto summaries = FetchAllDocumentSummaries();
  const auto expanded_ids = CaptureExpandedDocumentIds();
  last_document_summaries_ = summaries;
  if (workspace_tree_model_ != nullptr) {
    workspace_tree_model_->setWorkspaces(available_workspace_ids_);
    workspace_tree_model_->setDocuments(summaries);
  }
  RestoreExpandedDocumentIds(expanded_ids);
  RefreshConflictedDocumentIndicators();
}

void Page::RefreshPageListIfChanged() {
  const auto summaries = FetchAllDocumentSummaries();
  RefreshWorkspaceHydrationState();

  if (AreDocumentSummariesEqual(last_document_summaries_, summaries)) {
    return;
  }

  const auto expanded_ids = CaptureExpandedDocumentIds();
  last_document_summaries_ = summaries;
  if (workspace_tree_model_ != nullptr) {
    workspace_tree_model_->setWorkspaces(available_workspace_ids_);
    workspace_tree_model_->setDocuments(summaries);
  }
  RestoreExpandedDocumentIds(expanded_ids);
  ExpandWorkspace(current_workspace_id_);
  RefreshConflictedDocumentIndicators();
}

void Page::OnTreePressed(const QModelIndex& index) {
  if (workspace_tree_model_ == nullptr || !index.isValid()) {
    return;
  }

  if (workspace_tree_model_->isWorkspace(index)) {
    if (const auto workspace_id = WorkspaceIdFromIndex(index); workspace_id.has_value()) {
      ActivateWorkspace(*workspace_id);
    }
    return;
  }

  const auto doc_id = workspace_tree_model_->documentId(index);
  if (!doc_id) {
    return;
  }

  if (const auto workspace_id = WorkspaceIdFromIndex(index); workspace_id.has_value()) {
    ActivateWorkspace(*workspace_id);
  }
  selected_page_id_ = QString::fromStdString(*doc_id);
  OpenDocumentWithAccess(selected_page_id_);
}

void Page::CreateNewDocument(const QString& workspace_id) {
  if (editor_bridge_ == nullptr) {
    return;
  }

  ActivateWorkspace(workspace_id);
  const auto response = editor_bridge_->createDocumentInWorkspace(workspace_id);
  HandleCreatedDocument(response, workspace_id);
}

void Page::CreateChildDocument(const QModelIndex& parent_index) {
  if (editor_bridge_ == nullptr) {
    return;
  }

  const auto workspace_id = WorkspaceIdFromIndex(parent_index);
  if (!workspace_id.has_value()) {
    return;
  }

  if (workspace_tree_model_ != nullptr && workspace_tree_model_->isWorkspace(parent_index)) {
    CreateNewDocument(*workspace_id);
    return;
  }

  ActivateWorkspace(*workspace_id);
  const auto parent_id = MapToParentDocumentId(parent_index);
  if (!parent_id) {
    spdlog::warn("Cannot create child document: invalid parent index");
    return;
  }

  const auto response = editor_bridge_->createChildDocumentInWorkspace(
      *workspace_id, QString::fromStdString(*parent_id));
  HandleCreatedDocument(response, *workspace_id);
}

void Page::RenameDocument(const QModelIndex& index) {
  if (workspace_tree_model_ == nullptr || workspace_tree_view_ == nullptr ||
      editor_bridge_ == nullptr) {
    return;
  }

  const auto doc_id = workspace_tree_model_->documentId(index);
  if (!doc_id) {
    return;
  }

  const auto workspace_id = WorkspaceIdFromIndex(index);
  if (!workspace_id.has_value()) {
    return;
  }

  bool accepted = false;
  const auto current_title = index.data(Qt::DisplayRole).toString();
  const auto new_title =
      QInputDialog::getText(workspace_tree_view_, QStringLiteral("Rename title"),
                            QStringLiteral("Title:"), QLineEdit::Normal, current_title, &accepted)
          .trimmed();
  if (!accepted || new_title.isEmpty() || new_title == current_title) {
    return;
  }

  ActivateWorkspace(*workspace_id);
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

void Page::HandleCreatedDocument(const QVariantMap& response, const QString& workspace_id) {
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

  if (workspace_tree_model_ == nullptr) {
    return;
  }

  ActivateWorkspace(workspace_id);
  ExpandWorkspace(workspace_id);
  workspace_tree_model_->appendDocument(SummaryFromVariantMap(created));
  selected_page_id_ = page_id;
  ExpandAncestors(page_id);
  SelectDocumentById(page_id);
  OpenDocumentWithAccess(page_id);
}

void Page::SelectDocumentById(const QString& page_id) {
  if (page_id.isEmpty()) {
    return;
  }

  if (workspace_tree_model_ == nullptr || workspace_tree_view_ == nullptr) {
    return;
  }

  const auto index = workspace_tree_model_->indexForDocumentId(page_id.toStdString());
  if (!index.isValid()) {
    return;
  }

  ExpandAncestors(page_id);
  workspace_tree_view_->setCurrentIndex(index);
  workspace_tree_view_->scrollTo(index, QAbstractItemView::PositionAtCenter);
}

void Page::DeleteDocument(const QModelIndex& index) {
  if (workspace_tree_model_ == nullptr || editor_bridge_ == nullptr) {
    return;
  }

  const auto doc_id = workspace_tree_model_->documentId(index);
  if (!doc_id) {
    return;
  }

  const auto workspace_id = WorkspaceIdFromIndex(index);
  if (!workspace_id.has_value()) {
    return;
  }

  ActivateWorkspace(*workspace_id);
  const auto response = editor_bridge_->deleteDocument(QString::fromStdString(*doc_id));
  if (!response.value(QStringLiteral("ok")).toBool()) {
    const auto error = response.value(QStringLiteral("error")).toMap();
    spdlog::error("Failed to delete document: {}",
                  error.value(QStringLiteral("message")).toString().toStdString());
    return;
  }

  PopulatePageList();

  const auto summaries = FetchDocumentSummaries(*workspace_id);
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

void Page::ApplyConflictStateForDocument(const QString& page_id) {
  if (editor_bridge_ == nullptr || page_id.isEmpty()) {
    return;
  }

  bool has_conflict = false;
  if (context_.document_repository != nullptr) {
    const auto listed = context_.document_repository->ListConflicts();
    if (!listed.error) {
      for (const auto& conflict : listed.conflicts) {
        if (conflict.resolution_state == "pending" &&
            QString::fromStdString(conflict.document_id) == page_id) {
          has_conflict = true;
          if (page_id == selected_page_id_) {
            emit documentConflictDetected(page_id, QString::fromStdString(conflict.id));
          }
          break;
        }
      }
    }
  }

  if (page_id == selected_page_id_) {
    editor_bridge_->SetCurrentDocumentConflicted(has_conflict);
  }

  RefreshConflictedDocumentIndicators();
}

void Page::RefreshConflictedDocumentIndicators() {
  if (workspace_tree_model_ == nullptr || context_.document_repository == nullptr) {
    return;
  }

  QSet<QString> conflicted_ids;
  const auto listed = context_.document_repository->ListConflicts();
  if (!listed.error) {
    for (const auto& conflict : listed.conflicts) {
      if (conflict.resolution_state == "pending") {
        conflicted_ids.insert(QString::fromStdString(conflict.document_id));
      }
    }
  }
  workspace_tree_model_->setConflictedDocumentIds(conflicted_ids);
}

void Page::RefreshCurrentDocumentConflictState() {
  ApplyConflictStateForDocument(selected_page_id_);
}

void Page::RefreshSelectedDocumentAccess() {
  if (selected_page_id_.isEmpty()) {
    return;
  }

  // Background auth/sync updates must not reopen a document that is already in
  // edit mode. Reopening negotiates a fresh view-session and can downgrade the
  // current lock-backed edit session, which causes the editor to jump, lose
  // selection, or switch back to read-only while the user is typing.
  if (current_document_editable_) {
    return;
  }

  OpenDocumentWithAccess(selected_page_id_);
}

void Page::EnterEditMode() {
  if (selected_page_id_.isEmpty() || context_.backend_client == nullptr ||
      editor_bridge_ == nullptr) {
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
  if (selected_page_id_.isEmpty() || context_.backend_client == nullptr ||
      editor_bridge_ == nullptr) {
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
  SetEditModeEnabled(!current_document_editable_);
}

void Page::SetEditModeEnabled(bool enabled) {
  if (selected_page_id_.isEmpty()) {
    return;
  }

  if (enabled == current_document_editable_) {
    return;
  }

  if (enabled) {
    EnterEditMode();
  } else {
    ExitEditMode();
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
    emit collaborationStatusChanged(QStringLiteral("Collab: editing"),
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
        QStringLiteral(
            "Editing access was lost. Re-enter edit mode when the backend is reachable."),
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
  if (edit_inactivity_timer_ == nullptr || current_document_local_only_ ||
      !current_document_editable_) {
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
  if (workspace_tree_model_ == nullptr || editor_bridge_ == nullptr) {
    return;
  }

  const auto doc_id = workspace_tree_model_->documentId(index);
  if (!doc_id || delta == 0) {
    return;
  }

  const auto workspace_id = WorkspaceIdFromIndex(index);
  if (!workspace_id.has_value()) {
    return;
  }

  ActivateWorkspace(*workspace_id);
  auto summaries = FetchDocumentSummaries(*workspace_id);
  const auto target_it = std::find_if(summaries.begin(), summaries.end(),
                                      [&](const auto& summary) { return summary.id == *doc_id; });
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

  auto sibling_it = std::find_if(siblings.begin(), siblings.end(),
                                 [&](const auto& summary) { return summary.get().id == *doc_id; });
  if (sibling_it == siblings.end()) {
    return;
  }

  const auto position = static_cast<std::ptrdiff_t>(std::distance(siblings.begin(), sibling_it));
  const auto new_position = position + delta;
  if (new_position < 0 || new_position >= static_cast<std::ptrdiff_t>(siblings.size())) {
    return;
  }

  std::iter_swap(siblings.begin() + position, siblings.begin() + new_position);

  for (std::size_t i = 0; i < siblings.size(); ++i) {
    auto& summary = siblings[i].get();
    const auto response = editor_bridge_->updateDocumentPlacement(
        QString::fromStdString(summary.id),
        summary.parent_id ? QString::fromStdString(*summary.parent_id) : QString{},
        summary.parent_id.has_value(), static_cast<int>(i));
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

void Page::MoveDocumentToPlacement(const QString& source_document_id,
                                   const QString& target_parent_id, bool has_parent_id,
                                   int target_sort_order, const QString& workspace_id) {
  if (editor_bridge_ == nullptr || source_document_id.isEmpty()) {
    return;
  }

  if (has_parent_id && target_parent_id == source_document_id) {
    return;
  }

  ActivateWorkspace(workspace_id);
  auto summaries = FetchDocumentSummaries(workspace_id);
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
      const auto parent_it =
          std::find_if(summaries.begin(), summaries.end(),
                       [&](const auto& summary) { return summary.id == *cursor; });
      if (parent_it == summaries.end()) {
        break;
      }
      cursor = parent_it->parent_id;
    }
  }

  const auto old_parent = source_it->parent_id;
  const auto new_parent =
      has_parent_id ? std::make_optional(target_parent_id.toStdString()) : std::nullopt;

  auto build_group = [&](const std::optional<std::string>& parent_id, std::string_view skip_id) {
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
          parent_id ? QString::fromStdString(*parent_id) : QString{}, parent_id.has_value(),
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
  if (workspace_tree_model_ == nullptr || workspace_tree_view_ == nullptr) {
    return;
  }

  const auto index = workspace_tree_view_->indexAt(position);
  if (!index.isValid()) {
    return;
  }

  if (workspace_tree_model_->isWorkspace(index)) {
    workspace_tree_view_->setCurrentIndex(index);
    const auto workspace_id = WorkspaceIdFromIndex(index);
    if (!workspace_id.has_value()) {
      return;
    }

    auto* menu = new gui::DocumentContextMenu({.can_move_up = false, .can_move_down = false},
                                              workspace_tree_view_);
    connect(menu, &gui::DocumentContextMenu::actionRequested, this,
            [this, index](gui::DocumentContextMenu::Action action) {
              if (action == gui::DocumentContextMenu::Action::kAddChildPage) {
                CreateChildDocument(index);
              }
            });
    menu->ShowAt(workspace_tree_view_->mapToGlobal(position));
    return;
  }

  const auto doc_id = workspace_tree_model_->documentId(index);
  if (!doc_id) {
    return;
  }

  const auto workspace_id = WorkspaceIdFromIndex(index);
  if (!workspace_id.has_value()) {
    return;
  }

  ActivateWorkspace(*workspace_id);
  workspace_tree_view_->setCurrentIndex(index);
  spdlog::info("Context menu requested");

  const auto summaries = FetchDocumentSummaries(*workspace_id);
  const auto current_it = std::find_if(summaries.begin(), summaries.end(),
                                       [&](const auto& summary) { return summary.id == *doc_id; });

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

  const auto current_sibling_it =
      std::find_if(same_parent_siblings.begin(), same_parent_siblings.end(),
                   [&](const auto* summary) { return summary->id == *doc_id; });
  const bool can_move_up = current_sibling_it != same_parent_siblings.end() &&
                           current_sibling_it != same_parent_siblings.begin();
  const bool can_move_down = current_sibling_it != same_parent_siblings.end() &&
                             std::next(current_sibling_it) != same_parent_siblings.end();

  auto* menu = new gui::DocumentContextMenu(
      {.can_move_up = can_move_up, .can_move_down = can_move_down}, workspace_tree_view_);
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
  menu->ShowAt(workspace_tree_view_->mapToGlobal(position));
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
  if (workspace_tree_model_ == nullptr || workspace_tree_view_ == nullptr) {
    return expanded_ids;
  }

  VisitIndexes(workspace_tree_model_.get(), QModelIndex{}, [&](const QModelIndex& index) {
    if (!workspace_tree_view_->isExpanded(index)) {
      return;
    }

    if (workspace_tree_model_->isWorkspace(index)) {
      if (const auto workspace_id = WorkspaceIdFromIndex(index); workspace_id.has_value()) {
        expanded_ids.insert(MakeWorkspaceScopedDocumentId(*workspace_id, "__workspace__"));
      }
      return;
    }

    if (const auto doc_id = workspace_tree_model_->documentId(index); doc_id.has_value()) {
      const auto workspace_id = WorkspaceIdFromIndex(index);
      if (workspace_id.has_value()) {
        expanded_ids.insert(MakeWorkspaceScopedDocumentId(*workspace_id, *doc_id));
      }
    }
  });
  return expanded_ids;
}

void Page::RestoreExpandedDocumentIds(const std::set<std::string>& expanded_ids) {
  if (workspace_tree_model_ == nullptr || workspace_tree_view_ == nullptr) {
    return;
  }

  for (const auto& workspace_id : available_workspace_ids_) {
    const auto workspace_key = MakeWorkspaceScopedDocumentId(workspace_id, "__workspace__");
    if (expanded_ids.contains(workspace_key)) {
      ExpandWorkspace(workspace_id);
    }
  }

  for (const auto& scoped_id : expanded_ids) {
    const auto separator = scoped_id.find("::");
    if (separator == std::string::npos) {
      continue;
    }
    const auto local_id = scoped_id.substr(separator + 2);
    if (local_id == "__workspace__") {
      continue;
    }
    const auto index = workspace_tree_model_->indexForDocumentId(local_id);
    if (index.isValid()) {
      workspace_tree_view_->setExpanded(index, true);
    }
  }
}

void Page::ExpandAncestors(const QString& page_id) {
  if (page_id.isEmpty() || workspace_tree_model_ == nullptr || workspace_tree_view_ == nullptr) {
    return;
  }

  auto index = workspace_tree_model_->indexForDocumentId(page_id.toStdString());
  if (!index.isValid()) {
    return;
  }

  while (index.isValid()) {
    workspace_tree_view_->setExpanded(index, true);
    index = index.parent();
  }
}

void Page::ExpandWorkspace(const QString& workspace_id) {
  if (workspace_tree_model_ == nullptr || workspace_tree_view_ == nullptr ||
      workspace_id.isEmpty()) {
    return;
  }

  const auto workspace_index =
      workspace_tree_model_->indexForWorkspaceId(workspace_id.toStdString());
  if (workspace_index.isValid()) {
    workspace_tree_view_->setExpanded(workspace_index, true);
  }
}

std::vector<storage::DocumentSummary> Page::FetchAllDocumentSummaries() const {
  std::vector<storage::DocumentSummary> summaries;
  for (const auto& workspace_id : available_workspace_ids_) {
    auto workspace_summaries = FetchDocumentSummaries(workspace_id);
    std::move(workspace_summaries.begin(), workspace_summaries.end(),
              std::back_inserter(summaries));
  }
  return summaries;
}

std::vector<storage::DocumentSummary> Page::FetchDocumentSummaries(
    const QString& workspace_id) const {
  std::vector<storage::DocumentSummary> summaries;
  if (editor_bridge_ == nullptr) {
    return summaries;
  }

  const auto response = editor_bridge_->listDocumentsInWorkspace(workspace_id);
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
  if (!index.isValid() || workspace_tree_model_ == nullptr) {
    return std::nullopt;
  }

  return workspace_tree_model_->documentId(index);
}

std::optional<QString> Page::WorkspaceIdFromIndex(const QModelIndex& index) const {
  if (!index.isValid() || workspace_tree_model_ == nullptr) {
    return std::nullopt;
  }

  const auto workspace_id = workspace_tree_model_->workspaceId(index);
  if (!workspace_id.has_value() || workspace_id->empty()) {
    return std::nullopt;
  }
  return QString::fromStdString(*workspace_id);
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
