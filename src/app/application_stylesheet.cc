#include "app/application_stylesheet.h"

#include <array>

#include <QApplication>
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
      QDir::current().filePath(ToQString(constants::kApplicationQssPath)),
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../") +
                                                           ToQString(constants::kApplicationQssPath)),
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../../") +
                                                           ToQString(constants::kApplicationQssPath)),
  };
}

}  // namespace

auto ResolveApplicationStylesheetPath() -> QString {
  for (const auto& candidate : CandidatePaths()) {
    if (QFileInfo(candidate).exists()) {
      return candidate;
    }
  }
  return {};
}

void ApplyApplicationStylesheet() {
  const auto path = ResolveApplicationStylesheetPath();
  if (path.isEmpty()) {
    qApp->setStyleSheet(QString{});
    return;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qApp->setStyleSheet(QString{});
    return;
  }

  qApp->setStyleSheet(QString::fromUtf8(file.readAll()));
}

}  // namespace cppwiki
