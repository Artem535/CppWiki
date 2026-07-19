#include "gui/document_context_menu.h"

#include <QApplication>
#include <QAction>
#include <QMenu>
#include <QPushButton>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

// Finds the top-level "New document" button that opens the kind submenu.
QPushButton* FindNewDocumentButton(cppwiki::gui::DocumentContextMenu& menu) {
  for (auto* button : menu.findChildren<QPushButton*>()) {
    if (button->text() == QStringLiteral("New document")) {
      return button;
    }
  }
  return nullptr;
}

// Clicks the "New document" button (opening the submenu synchronously via
// QMenu::popup()), then finds and triggers the submenu QAction with the given
// text. Returns the DocumentKind captured by newDocumentRequested, if any.
std::optional<cppwiki::document::DocumentKind> TriggerSubmenuChoice(
    cppwiki::gui::DocumentContextMenu& menu, const QString& choice_text) {
  auto* button = FindNewDocumentButton(menu);
  if (button == nullptr) {
    return std::nullopt;
  }

  std::optional<cppwiki::document::DocumentKind> captured_kind;
  QObject::connect(&menu, &cppwiki::gui::DocumentContextMenu::newDocumentRequested, &menu,
                   [&captured_kind](cppwiki::document::DocumentKind kind) {
                     captured_kind = kind;
                   });

  button->click();  // Synchronously runs ShowNewDocumentSubmenu(), which calls QMenu::popup().

  auto* submenu = menu.findChild<QMenu*>();
  if (submenu == nullptr) {
    return std::nullopt;
  }

  for (auto* action : submenu->actions()) {
    if (action->text() == choice_text) {
      action->trigger();
      return captured_kind;
    }
  }
  return std::nullopt;
}

void TestNewDocumentSubmenuOffersThreeKindChoices() {
  cppwiki::gui::DocumentContextMenu menu({.can_move_up = false, .can_move_down = false});

  auto* button = FindNewDocumentButton(menu);
  Require(button != nullptr, "context menu should expose a 'New document' button");

  button->click();
  auto* submenu = menu.findChild<QMenu*>();
  Require(submenu != nullptr, "clicking 'New document' should open a submenu");

  const auto actions = submenu->actions();
  Require(actions.size() == 3, "submenu should offer exactly three kind choices");
  Require(actions.at(0)->text() == QStringLiteral("Wiki page"),
          "'Wiki page' must be the first/default submenu choice");
  Require(actions.at(1)->text() == QStringLiteral("Jupyter notebook"),
          "second submenu choice should be 'Jupyter notebook'");
  Require(actions.at(2)->text() == QStringLiteral("Excalidraw canvas"),
          "third submenu choice should be 'Excalidraw canvas'");
}

void TestEachSubmenuChoiceEmitsCorrectDocumentKind() {
  {
    cppwiki::gui::DocumentContextMenu menu({.can_move_up = false, .can_move_down = false});
    const auto kind = TriggerSubmenuChoice(menu, QStringLiteral("Wiki page"));
    Require(kind.has_value() && *kind == cppwiki::document::DocumentKind::kWikiPage,
            "'Wiki page' choice should emit newDocumentRequested(kWikiPage)");
  }
  {
    cppwiki::gui::DocumentContextMenu menu({.can_move_up = false, .can_move_down = false});
    const auto kind = TriggerSubmenuChoice(menu, QStringLiteral("Jupyter notebook"));
    Require(kind.has_value() && *kind == cppwiki::document::DocumentKind::kJupyterNotebook,
            "'Jupyter notebook' choice should emit newDocumentRequested(kJupyterNotebook)");
  }
  {
    cppwiki::gui::DocumentContextMenu menu({.can_move_up = false, .can_move_down = false});
    const auto kind = TriggerSubmenuChoice(menu, QStringLiteral("Excalidraw canvas"));
    Require(kind.has_value() && *kind == cppwiki::document::DocumentKind::kExcalidrawCanvas,
            "'Excalidraw canvas' choice should emit newDocumentRequested(kExcalidrawCanvas)");
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
  QApplication application(argc, argv);

  TestNewDocumentSubmenuOffersThreeKindChoices();
  TestEachSubmenuChoiceEmitsCorrectDocumentKind();

  return EXIT_SUCCESS;
}
