#ifndef CPPWIKI_SRC_GUI_DOCUMENT_CONTEXT_MENU_H_
#define CPPWIKI_SRC_GUI_DOCUMENT_CONTEXT_MENU_H_

#include <QFrame>
#include <QIcon>
#include <QList>
#include <QPoint>
#include <QString>

#include "document/document.h"

class QWidget;
class QPushButton;

namespace cppwiki::gui {

class DocumentContextMenu final : public QFrame {
  Q_OBJECT

 public:
  enum class Action {
    kRenameTitle,
    kMoveUp,
    kMoveDown,
    kDeletePage,
  };
  Q_ENUM(Action)

  struct Options {
    bool can_move_up = false;
    bool can_move_down = false;
  };

  explicit DocumentContextMenu(const Options& options, QWidget* parent = nullptr);

  void ShowAt(const QPoint& global_position);

 signals:
  void actionRequested(cppwiki::gui::DocumentContextMenu::Action action);

  // Emitted when a specific kind was chosen from the "New document" options
  // (Wiki page / Jupyter notebook / Excalidraw canvas). `kind` defaults to
  // DocumentKind::kWikiPage when "Wiki page" is chosen, matching today's
  // single hardcoded "Add child page" behavior byte-for-byte.
  void newDocumentRequested(document::DocumentKind kind);

 private:
  void AddButton(Action action, const QString& text, const QIcon& icon, bool enabled);
  // Adds the "New document" toggle and its three (initially hidden) kind
  // options, expanded/collapsed inline within this same popup's layout —
  // see the .cc file for why a nested QMenu submenu doesn't work here.
  void AddNewDocumentOptions();
  void AddKindOption(document::DocumentKind kind, const QString& text);

  QList<QPushButton*> new_document_kind_buttons_;
};

}  // namespace cppwiki::gui

#endif  // CPPWIKI_SRC_GUI_DOCUMENT_CONTEXT_MENU_H_
