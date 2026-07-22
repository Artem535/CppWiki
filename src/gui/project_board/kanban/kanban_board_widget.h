#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_BOARD_WIDGET_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_BOARD_WIDGET_H_

#include <QByteArray>
#include <QWidget>

#include "gui/project_board/kanban/kanban_board_model.h"

class QQuickWidget;

namespace cppwiki::gui::kanban {

// Embeds the native Kanban board (KanbanBoard.qml) into a QWidget-based UI via QQuickWidget --
// this app is otherwise QWidgets-based, so this is the seam between the two. Wraps the QQuickWidget
// with a thin native toolbar (Add column/Add task) and a task-edit dialog (see
// kanban_task_dialog.h), since QML has no business owning dialog UI or the app's dark-theme
// widget chrome -- those stay native, only the drag/drop card grid itself is QML.
class KanbanBoardWidget final : public QWidget {
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
  void HandleAddColumnClicked();
  void HandleAddTaskClicked();
  void HandleEditTaskRequested(const QString& task_id);

  KanbanBoardModel* model_;
  QQuickWidget* quick_widget_ = nullptr;
};

}  // namespace cppwiki::gui::kanban

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_BOARD_WIDGET_H_
