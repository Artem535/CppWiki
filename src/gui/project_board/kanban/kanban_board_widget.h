#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_BOARD_WIDGET_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_BOARD_WIDGET_H_

#include <QByteArray>
#include <QQuickWidget>
#include <QWidget>

#include "gui/project_board/kanban/kanban_board_model.h"

namespace cppwiki::gui::kanban {

// Embeds the native Kanban board (KanbanBoard.qml) into a QWidget-based UI via QQuickWidget —
// this app is otherwise QWidgets-based, so this is the seam between the two. Standalone for now
// (see gui/project_board/kanban/demo); not wired into cppwiki_app / MainWindow / Page yet.
class KanbanBoardWidget final : public QQuickWidget {
  Q_OBJECT

 public:
  explicit KanbanBoardWidget(QWidget* parent = nullptr);

  // Replaces the board's contents with the `{ tasks, columns }` document in `json`. Invalid JSON
  // is ignored (the board keeps whatever it had before).
  void LoadFromJson(const QByteArray& json);

  // Serializes the current board state back to the same `{ tasks, columns }` shape.
  [[nodiscard]] auto ToJson() const -> QByteArray;

  [[nodiscard]] auto Model() -> KanbanBoardModel*;

 private:
  KanbanBoardModel* model_;
};

}  // namespace cppwiki::gui::kanban

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_BOARD_WIDGET_H_
