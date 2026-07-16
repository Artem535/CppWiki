#include "app/application_stylesheet.h"

#include <array>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QWidget>

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

void ApplyApplicationStylesheet(QWidget* target) {
  if (target == nullptr) {
    return;
  }

  const auto path = ResolveApplicationStylesheetPath();
  if (path.isEmpty()) {
    target->setStyleSheet(QString{});
    return;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    target->setStyleSheet(QString{});
    return;
  }

  target->setStyleSheet(QString::fromUtf8(file.readAll()));
}

}  // namespace cppwiki
