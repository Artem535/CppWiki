#include "gui/page.h"

#include <QFileInfo>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineView>

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

}  // namespace

Page::Page(QWidget* parent) : QWidget(parent) {
  BuildUi();
}

Page::~Page() = default;

QString Page::Title() const {
  return QStringLiteral("Getting Started");
}

QWidget* Page::Widget() {
  return this;
}

void Page::BuildUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  editor_view_ = new QWebEngineView(this);
  layout->addWidget(editor_view_);

  LoadEditor();
}

void Page::LoadEditor() {
  const QString editor_index_path =
      QStringLiteral(CPPWIKI_EDITOR_DIST_DIR) + QStringLiteral("/index.html");
  const QFileInfo editor_index(editor_index_path);

  if (editor_index.exists() && editor_index.isFile()) {
    editor_view_->load(QUrl::fromLocalFile(editor_index.absoluteFilePath()));
    return;
  }

  editor_view_->setHtml(EditorFallbackHtml(editor_index.absoluteFilePath()));
}

}  // namespace cppwiki
