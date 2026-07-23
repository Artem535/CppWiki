#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_TASK_DIALOG_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_TASK_DIALOG_H_

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

#include "gui/project_board/kanban/kanban_column.h"
#include "gui/project_board/kanban/kanban_task.h"

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

namespace cppwiki::gui::kanban {

// Native replacement for the create/edit-task forms the web Kanban's card menu used to open (see
// ProjectBoardView.tsx) — the "add a task", "add an epic", and "edit this card" toolbar/gesture
// entry points in KanbanBoardWidget all funnel through this one form (status/priority/progress/
// epic/tags/assignees/description), just seeded differently. An epic is a task with
// type == "summary" (KanbanTask::IsEpic()) rather than a distinct kind of object, so it gets the
// same form with the "Epic" checkbox pre-checked and the status field disabled (epics are
// swimlane headers, not per-column cards).
class KanbanTaskDialog final : public QDialog {
  Q_OBJECT

 public:
  struct Result {
    QString text;
    QString column_id;
    int priority = 0;  // 0 = none; kPriorityLow/Medium/High otherwise.
    int progress = 0;
    bool is_epic = false;
    QString description;
    QStringList tags;
    QStringList users;
    // ISO 8601 UTC date-time string (see KanbanTask::start) and duration in days -- matches the
    // Table view's own Start/Duration editing (ProjectDateEditDelegate / numeric spin delegate),
    // rather than an explicit end date, since a task's end is normally derived from these two.
    QString start;
    double duration = 1;
  };

  // Shows a modal dialog seeded with empty/default fields (status defaults to `columns`' first
  // entry). `default_is_epic` pre-checks the "Epic" box -- used by the toolbar's "Add epic"
  // button, as opposed to "Add task" which leaves it unchecked. Returns std::nullopt if the user
  // cancels or leaves the task text blank.
  [[nodiscard]] static auto RequestNewTask(QWidget* parent, const QVector<KanbanColumn>& columns,
                                           bool default_is_epic = false) -> std::optional<Result>;

  // Shows a modal dialog seeded from `task`'s current fields. Returns std::nullopt if the user
  // cancels or leaves the task text blank.
  [[nodiscard]] static auto RequestEditTask(QWidget* parent, const QVector<KanbanColumn>& columns,
                                            const KanbanTask& task) -> std::optional<Result>;

 private:
  KanbanTaskDialog(QWidget* parent, const QVector<KanbanColumn>& columns);

  void SetInitialValues(const KanbanTask& task);
  [[nodiscard]] auto ToResult() const -> Result;

  QLineEdit* text_edit_ = nullptr;
  QCheckBox* epic_check_ = nullptr;
  QComboBox* column_combo_ = nullptr;
  QComboBox* priority_combo_ = nullptr;
  QSpinBox* progress_spin_ = nullptr;
  QDateEdit* start_edit_ = nullptr;
  QSpinBox* duration_spin_ = nullptr;
  QLineEdit* tags_edit_ = nullptr;
  QLineEdit* users_edit_ = nullptr;
  QPlainTextEdit* description_edit_ = nullptr;
};

}  // namespace cppwiki::gui::kanban

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_TASK_DIALOG_H_
