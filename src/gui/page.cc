#include "gui/page.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <set>

#include <QAbstractItemView>
#include <QAction>
#include <QIcon>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QMenu>
#include <QPushButton>
#include <QSize>
#include <QVBoxLayout>
#include <QSplitter>
#include <QTreeView>
#include <QUrl>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineView>
#include <utility>

#include "bridge/editor_bridge.h"
#include "core/constants.h"
#include "core/qt_string.h"
#include "gui/document_tree_item_delegate.h"
#include "gui/document_tree_model.h"
#include "gui/document_tree_view.h"

namespace cppwiki {
namespace {

auto EditorFallbackHtml(const QString& expected_path) -> QString {
  return QStringLiteral(R"(
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <meta
      name="viewport"
      content="width=device-width, initial-scale=1"
    >
    <style>
      body {
        margin: 0;
        font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
        color: #1f2933;
        background: #f7f8fa;
      }
      main {
        box-sizing: border-box;
        min-height: 100vh;
        padding: 48px;
      }
      h1 {
        margin: 0 0 12px;
        font-size: 28px;
        font-weight: 600;
      }
      p {
        max-width: 720px;
        margin: 0;
        line-height: 1.55;
      }
      code {
        border-radius: 4px;
        background: #e5e7eb;
        padding: 2px 5px;
      }
    </style>
    <title>CppWiki Editor Host</title>
  </head>
  <body>
    <main>
      <h1>CppWiki Editor Host</h1>
      <p>
        QWebEngine is running, but the BlockNote editor bundle has not been built yet.
        Run <code>npm ci</code> and <code>npm run build</code> in <code>frontend/editor</code>.
      </p>
      <p style="margin-top: 16px;">Expected bundle: <code>%1</code></p>
    </main>
  </body>
</html>
)")
      .arg(expected_path.toHtmlEscaped());
}


auto OptionalParentId(const QVariant& value) -> std::optional<std::string> {
  if (!value.isValid() || value.isNull()) {
    return std::nullopt;
  }
  const auto str = value.toString();
  if (str.isEmpty()) {
    return std::nullopt;
  }
  return str.toStdString();
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
  return this;
}

void Page::BuildUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

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

  auto* splitter = new QSplitter(Qt::Horizontal, this);
  page_panel_ = new QWidget(this);
  page_panel_->setObjectName(QStringLiteral("pagePanel"));
  page_panel_->setAttribute(Qt::WA_StyledBackground, true);
  page_panel_->setStyleSheet(QStringLiteral(R"(
    QWidget#pagePanel {
      background-color: palette(base);
      border-radius: 10px;
    }
    QWidget#pagePanel QTreeView,
    QWidget#pagePanel QTreeView::viewport {
      background: transparent;
      border: none;
    }
  )"));
  auto* page_panel_layout = new QVBoxLayout(page_panel_);
  page_panel_layout->setContentsMargins(12, 12, 12, 12);
  page_panel_layout->setSpacing(8);

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
  splitter->addWidget(page_panel_);
  splitter->addWidget(editor_view_);
  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);
  splitter->setSizes({constants::kPageListInitialWidth,
                      width() - constants::kPageListInitialWidth});

  layout->addWidget(splitter, 1);

  LoadEditor();
  PopulatePageList();
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

  // Open documents from normal row clicks. Inline tree actions are handled by DocumentTreeView.
  connect(page_tree_, &QTreeView::clicked, this, [this](const QModelIndex& index) {
    if (index.isValid()) {
      OnTreePressed(index);
    }
  });
  connect(page_tree_, &QWidget::customContextMenuRequested, this, [this](const QPoint& position) {
    ShowContextMenu(position);
  });

  connect(document_tree_view, &gui::DocumentTreeView::addChildRequested, this,
          [this](const QModelIndex& parent_index) {
            CreateChildDocument(parent_index);
          });
  connect(editor_bridge_, &bridge::QEditorBridge::saveStatusChanged, this,
          [this](const QString& page_id, bool success, const QString&) {
            HandleDocumentSaved(page_id, success);
          });
}

void Page::LoadEditor() {
  const QString editor_index_path = context_.settings.EditorDistDirectory() + QStringLiteral("/index.html");
  const QFileInfo editor_index(editor_index_path);

  if (editor_index.exists() && editor_index.isFile()) {
    editor_view_->load(QUrl::fromLocalFile(editor_index.absoluteFilePath()));
    return;
  }

  editor_view_->setHtml(EditorFallbackHtml(editor_index.absoluteFilePath()));
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

  // Convert QVariantList to DocumentSummary vector
  std::vector<storage::DocumentSummary> summaries;
  summaries.reserve(static_cast<size_t>(documents.size()));

  for (const auto& document_value : documents) {
    const auto document = document_value.toMap();
    storage::DocumentSummary summary;
    summary.id = document.value(QStringLiteral("id")).toString().toStdString();
    summary.title = document.value(QStringLiteral("title")).toString().toStdString();
    summary.parent_id = OptionalParentId(document.value(QStringLiteral("parentId")));
    summary.sort_order = document.value(QStringLiteral("sortOrder")).toInt();
    summary.created_at = document.value(QStringLiteral("createdAt")).toString().toStdString();
    summary.updated_at = document.value(QStringLiteral("updatedAt")).toString().toStdString();

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
  editor_bridge_->RequestOpenDocument(QString::fromStdString(*doc_id));
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
  editor_bridge_->RequestOpenDocument(page_id);
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
    editor_bridge_->ClearCurrentDocumentSelection();
    return;
  }

  selected_page_id_ = QString::fromStdString(summaries.front().id);
  SelectDocumentById(selected_page_id_);
  editor_bridge_->RequestOpenDocument(selected_page_id_);
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

void Page::ShowContextMenu(const QPoint& position) {
  const auto index = page_tree_->indexAt(position);
  const auto doc_id = tree_model_->documentId(index);
  if (!doc_id) {
    return;
  }

  QMenu menu(page_tree_);
  auto* add_child_action = menu.addAction(QStringLiteral("Add child page"));
  auto* move_up_action = menu.addAction(QStringLiteral("Move up"));
  auto* move_down_action = menu.addAction(QStringLiteral("Move down"));
  menu.addSeparator();
  auto* delete_action = menu.addAction(QStringLiteral("Delete page"));

  QAction* chosen = menu.exec(page_tree_->viewport()->mapToGlobal(position));
  if (chosen == add_child_action) {
    CreateChildDocument(index);
  } else if (chosen == move_up_action) {
    MoveDocument(index, -1);
  } else if (chosen == move_down_action) {
    MoveDocument(index, 1);
  } else if (chosen == delete_action) {
    DeleteDocument(index);
  }
}

void Page::HandleDocumentSaved(const QString& page_id, bool success) {
  if (!success || selected_page_id_.isEmpty() || page_id != selected_page_id_) {
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
  summary.id = document.value(QStringLiteral("id")).toString().toStdString();
  summary.title = document.value(QStringLiteral("title")).toString().toStdString();
  summary.parent_id = OptionalParentId(document.value(QStringLiteral("parentId")));
  summary.sort_order = document.value(QStringLiteral("sortOrder")).toInt();
  summary.created_at = document.value(QStringLiteral("createdAt")).toString().toStdString();
  summary.updated_at = document.value(QStringLiteral("updatedAt")).toString().toStdString();
  return summary;
}

}  // namespace cppwiki
