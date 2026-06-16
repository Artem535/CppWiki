#include "gui/page.h"

#include <spdlog/spdlog.h>

#include <QAbstractItemView>
#include <QIcon>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
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

  // The tree model already provides a "New note" action as the first root row,
  // so keep only that single button inside the tree. A separate top-level button
  // duplicates functionality and visually clutters the panel.

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
  auto* page_panel_layout = new QVBoxLayout(page_panel_);
  page_panel_layout->setContentsMargins(0, 0, 0, 0);
  page_panel_layout->setSpacing(0);

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

  // Open documents from normal row clicks. Inline tree actions are handled by DocumentTreeView.
  connect(page_tree_, &QTreeView::clicked, this, [this](const QModelIndex& index) {
    if (index.isValid()) {
      OnTreePressed(index);
    }
  });

  connect(document_tree_view, &gui::DocumentTreeView::addChildRequested, this,
          [this](const QModelIndex& parent_index) {
            CreateChildDocument(parent_index);
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
  page_tree_->expandAll();
}

void Page::OnTreePressed(const QModelIndex& index) {
  if (index.data(gui::DocumentTreeModel::kAddChildActionRole).toBool()) {
    emit tree_model_->addChildRequested(index);
    return;
  }

  const auto doc_id = tree_model_->documentId(index);
  if (!doc_id) {
    if (index.data(gui::DocumentTreeModel::kIsActionRole).toBool()) {
      CreateNewDocument();
    }
    return;
  }

  editor_bridge_->RequestOpenDocument(QString::fromStdString(*doc_id));
}

void Page::OpenPage(const QModelIndex& index) {
  if (!index.isValid()) {
    return;
  }

  const auto doc_id = tree_model_->documentId(index);
  if (!doc_id) {
    return;
  }

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
  SelectDocumentById(page_id);
  editor_bridge_->RequestOpenDocument(page_id);
}

void Page::SelectDocumentById(const QString& page_id) {
  if (!tree_model_ || page_id.isEmpty()) {
    return;
  }

  const auto index = tree_model_->indexForDocumentId(page_id.toStdString());
  if (index.isValid()) {
    page_tree_->setCurrentIndex(index);
    page_tree_->scrollTo(index, QAbstractItemView::PositionAtCenter);
  }
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
