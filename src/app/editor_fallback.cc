#include "app/editor_fallback.h"

#include <array>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>

#include "core/constants.h"
#include "core/qt_string.h"

namespace cppwiki {

namespace {

auto CandidatePaths() -> std::array<QString, 3> {
  return {
      QDir::current().filePath(ToQString(constants::kEditorFallbackHtmlPath)),
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../") +
                                                           ToQString(constants::kEditorFallbackHtmlPath)),
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../../") +
                                                           ToQString(constants::kEditorFallbackHtmlPath)),
  };
}

}  // namespace

auto ResolveEditorFallbackHtmlPath() -> QString {
  for (const auto& candidate : CandidatePaths()) {
    if (QFileInfo(candidate).exists()) {
      return candidate;
    }
  }
  return {};
}

auto LoadEditorFallbackHtml(const QString& expected_path) -> QString {
  const auto path = ResolveEditorFallbackHtmlPath();
  if (path.isEmpty()) {
    return QString{};
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QString{};
  }

  return QString::fromUtf8(file.readAll()).arg(expected_path.toHtmlEscaped());
}

}  // namespace cppwiki
