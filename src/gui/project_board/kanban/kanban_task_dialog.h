#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_TASK_DIALOG_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_TASK_DIALOG_H_

#include <QDialog>
#include <QString>
#include <QVector>
#include <optional>

#include "gui/project_board/kanban/kanban_column.h"
#include "gui/project_board/kanban/kanban_task.h"

class QComboBox;
class QLineEdit;
class QSpinBox;

namespace cppwiki::gui::kanban {

// Native replacement for the create/edit-task forms the web Kanban's card menu used to open (see
// ProjectBoardView.tsx) — the "add a task" and "edit this card" toolbar/gesture entry points in
// KanbanBoardWidget both funnel through this one form (status/priority/progress), just seeded
// differently.
class KanbanTaskDialog final : public QDialog {
  Q_OBJECT

 public:
  struct Result {
    QString text;
    QString column_id;
    int priority = 0;  // 0 = none; kPriorityLow/Medium/High otherwise.
    int progress = 0;
  };

  // Shows a modal dialog seeded with empty/default fields (status defaults to `columns`' first
  // entry). Returns std::nullopt if the user cancels or leaves the task text blank.
  [[nodiscard]] static auto RequestNewTask(QWidget* parent, const QVector<KanbanColumn>& columns)
      -> std::optional<Result>;

  // Shows a modal dialog seeded from `task`'s current fields. Returns std::nullopt if the user
  // cancels or leaves the task text blank.
  [[nodiscard]] static auto RequestEditTask(QWidget* parent, const QVector<KanbanColumn>& columns,
                                            const KanbanTask& task) -> std::optional<Result>;

 private:
  KanbanTaskDialog(QWidget* parent, const QVector<KanbanColumn>& columns);

  void SetInitialValues(const KanbanTask& task);
  [[nodiscard]] auto ToResult() const -> Result;

  QLineEdit* text_edit_ = nullptr;
  QComboBox* column_combo_ = nullptr;
  QComboBox* priority_combo_ = nullptr;
  QSpinBox* progress_spin_ = nullptr;
};

}  // namespace cppwiki::gui::kanban

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_TASK_DIALOG_H_
