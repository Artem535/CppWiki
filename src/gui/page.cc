#include "gui/page.h"

#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QVBoxLayout>
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

Page::Page(ProgramSettings settings, QWidget* parent)
    : QWidget(parent), settings_(std::move(settings)) {
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

  // Create the QWebEngineView and QWebChannel.
  channel_ = new QWebChannel(this);
  editor_bridge_ = new bridge::QEditorBridge(this);
  channel_->registerObject(ToQString(constants::kDocumentsBridgeObjectName), editor_bridge_);

  editor_view_ = new QWebEngineView(this);
  editor_view_->page()->setWebChannel(channel_);
  InstallWebChannelScript();

  layout->addWidget(editor_view_);

  LoadEditor();
}

void Page::LoadEditor() {
  const QString editor_index_path = settings_.EditorDistDirectory() + QStringLiteral("/index.html");
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

}  // namespace cppwiki
