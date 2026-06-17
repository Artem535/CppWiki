#ifndef CPPWIKI_SRC_GUI_DOCUMENT_CONTEXT_MENU_H_
#define CPPWIKI_SRC_GUI_DOCUMENT_CONTEXT_MENU_H_

#include <QFrame>
#include <QIcon>
#include <QPoint>
#include <QString>

class QWidget;

namespace cppwiki::gui {

class DocumentContextMenu final : public QFrame {
  Q_OBJECT

 public:
  enum class Action {
    kAddChildPage,
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

 private:
  void AddButton(Action action, const QString& text, const QIcon& icon, bool enabled);
};

}  // namespace cppwiki::gui

#endif  // CPPWIKI_SRC_GUI_DOCUMENT_CONTEXT_MENU_H_
