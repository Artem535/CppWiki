#include "app/application_stylesheet.h"

#include <array>

#include <QColor>
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

// Formats `color` as a QSS rgba(...) literal at the given alpha (0.0-1.0), matching the style
// already used throughout app/cppwiki.qss (e.g. collaborationPanel's semantic-state colors).
auto ToRgbaString(const QColor& color, double alpha) -> QString {
  return QStringLiteral("rgba(%1, %2, %3, %4)")
      .arg(color.red())
      .arg(color.green())
      .arg(color.blue())
      .arg(alpha, 0, 'f', 2);
}

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

auto BuildAccentColorStylesheet(AccentColor accent_color) -> QString {
  const auto base_color = AccentColorBaseColor(accent_color);
  // Only the rail's active-mode highlight is expressed as QSS here. The other ADR-016 accent
  // application — collaborationPanel[collaborationState="viewing"]'s tint — can't be QSS: since
  // PR #54, collaboration_panel_ is an ancestor of edit_mode_switch_ and so never receives any
  // stylesheet at all (see MainWindow::ApplyStylesheetToSafeDescendants()'s comment). That tint
  // is instead painted directly by CollaborationPanelFrame (main_window.cc), which reads the
  // accent color via MainWindow::ApplyAccentColor().
  return QStringLiteral(
             "QWidget#workspaceRailWidget QToolButton:checked {\n"
             "  background-color: %1;\n"
             "  border: 1px solid %2;\n"
             "  border-radius: 8px;\n"
             "}\n")
      .arg(ToRgbaString(base_color, 0.16), ToRgbaString(base_color, 0.4));
}

void ApplyApplicationStylesheet(QWidget* target, AccentColor accent_color) {
  if (target == nullptr) {
    return;
  }

  const auto path = ResolveApplicationStylesheetPath();
  if (path.isEmpty()) {
    target->setStyleSheet(BuildAccentColorStylesheet(accent_color));
    return;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    target->setStyleSheet(BuildAccentColorStylesheet(accent_color));
    return;
  }

  target->setStyleSheet(QString::fromUtf8(file.readAll()) + QStringLiteral("\n") +
                        BuildAccentColorStylesheet(accent_color));
}

}  // namespace cppwiki
