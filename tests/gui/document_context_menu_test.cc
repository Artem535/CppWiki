#include "gui/document_context_menu.h"

#include <QApplication>
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

QPushButton* FindButtonByText(cppwiki::gui::DocumentContextMenu& menu, const QString& text) {
  for (auto* button : menu.findChildren<QPushButton*>()) {
    if (button->text().trimmed() == text) {
      return button;
    }
  }
  return nullptr;
}

// Clicks the "New document" toggle (expanding the three inline kind options),
// then clicks the option matching `choice_text`. Returns the DocumentKind
// captured by newDocumentRequested, if any.
std::optional<cppwiki::document::DocumentKind> TriggerKindChoice(
    cppwiki::gui::DocumentContextMenu& menu, const QString& choice_text) {
  auto* toggle = FindButtonByText(menu, QStringLiteral("New document"));
  if (toggle == nullptr) {
    return std::nullopt;
  }

  std::optional<cppwiki::document::DocumentKind> captured_kind;
  QObject::connect(
      &menu, &cppwiki::gui::DocumentContextMenu::newDocumentRequested, &menu,
      [&captured_kind](cppwiki::document::DocumentKind kind) { captured_kind = kind; });

  toggle->click();  // Expands the inline kind options synchronously.

  auto* choice_button = FindButtonByText(menu, choice_text);
  if (choice_button == nullptr || !choice_button->isVisible()) {
    return std::nullopt;
  }
  choice_button->click();
  return captured_kind;
}

void TestNewDocumentToggleExpandsThreeKindChoices() {
  cppwiki::gui::DocumentContextMenu menu({.can_move_up = false, .can_move_down = false});
  menu.ShowAt(QPoint(0, 0));

  auto* toggle = FindButtonByText(menu, QStringLiteral("New document"));
  Require(toggle != nullptr, "context menu should expose a 'New document' toggle");

  Require(FindButtonByText(menu, QStringLiteral("Wiki page")) != nullptr,
          "'Wiki page' option should exist (hidden) before expanding");
  Require(!FindButtonByText(menu, QStringLiteral("Wiki page"))->isVisible(),
          "kind options should start hidden");

  toggle->click();

  auto* wiki_page = FindButtonByText(menu, QStringLiteral("Wiki page"));
  auto* jupyter_notebook = FindButtonByText(menu, QStringLiteral("Jupyter notebook"));
  auto* excalidraw_canvas = FindButtonByText(menu, QStringLiteral("Excalidraw canvas"));
  Require(wiki_page != nullptr && wiki_page->isVisible(),
          "'Wiki page' must be visible after expanding, and is the first/default option");
  Require(jupyter_notebook != nullptr && jupyter_notebook->isVisible(),
          "'Jupyter notebook' must be visible after expanding");
  Require(excalidraw_canvas != nullptr && excalidraw_canvas->isVisible(),
          "'Excalidraw canvas' must be visible after expanding");

  toggle->click();
  Require(!wiki_page->isVisible(), "clicking the toggle again should collapse the options");
}

void TestEachKindChoiceEmitsCorrectDocumentKind() {
  {
    cppwiki::gui::DocumentContextMenu menu({.can_move_up = false, .can_move_down = false});
    menu.ShowAt(QPoint(0, 0));
    const auto kind = TriggerKindChoice(menu, QStringLiteral("Wiki page"));
    Require(kind.has_value() && *kind == cppwiki::document::DocumentKind::kWikiPage,
            "'Wiki page' choice should emit newDocumentRequested(kWikiPage)");
  }
  {
    cppwiki::gui::DocumentContextMenu menu({.can_move_up = false, .can_move_down = false});
    menu.ShowAt(QPoint(0, 0));
    const auto kind = TriggerKindChoice(menu, QStringLiteral("Jupyter notebook"));
    Require(kind.has_value() && *kind == cppwiki::document::DocumentKind::kJupyterNotebook,
            "'Jupyter notebook' choice should emit newDocumentRequested(kJupyterNotebook)");
  }
  {
    cppwiki::gui::DocumentContextMenu menu({.can_move_up = false, .can_move_down = false});
    menu.ShowAt(QPoint(0, 0));
    const auto kind = TriggerKindChoice(menu, QStringLiteral("Excalidraw canvas"));
    Require(kind.has_value() && *kind == cppwiki::document::DocumentKind::kExcalidrawCanvas,
            "'Excalidraw canvas' choice should emit newDocumentRequested(kExcalidrawCanvas)");
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
  QApplication application(argc, argv);

  TestNewDocumentToggleExpandsThreeKindChoices();
  TestEachKindChoiceEmitsCorrectDocumentKind();

  return EXIT_SUCCESS;
}
