#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_BOARD_MODEL_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_BOARD_MODEL_H_

#include <QObject>
#include <QString>
#include <QVariantList>
#include <optional>

#include "gui/project_board/kanban/kanban_board_document.h"
#include "gui/project_board/kanban/kanban_task.h"

namespace cppwiki::gui::kanban {

// QML-facing model for the native Kanban board. Owns the in-memory task/column list and exposes
// it to QML as a swimlane x column grid (see rows()), so the QML side never has to re-derive the
// epic grouping itself — it just repeats over what this model already computed.
//
// Swimlane rules (the whole point of building Kanban natively — see the parent issue): a task
// with type == "summary" is an epic and gets its own swimlane; a non-epic task whose `parent`
// matches an epic's id belongs to that epic's swimlane; everything else lands in the trailing
// "No epic" swimlane (kNoEpicSwimlaneId). Epic tasks themselves are swimlane headers, not cards —
// they don't appear in any column.
class KanbanBoardModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList columns READ columns NOTIFY boardChanged)
  Q_PROPERTY(QVariantList rows READ rows NOTIFY boardChanged)

 public:
  // Sentinel swimlane id for tasks with no matching epic. Never a valid task id (task ids are
  // always non-empty in a well-formed document), so it can't collide with a real epic id.
  static constexpr auto kNoEpicSwimlaneId = "";

  explicit KanbanBoardModel(QObject* parent = nullptr);

  void SetDocument(KanbanBoardDocument document);
  [[nodiscard]] auto ExportDocument() const -> KanbanBoardDocument;

  [[nodiscard]] auto columns() const -> QVariantList;
  [[nodiscard]] auto rows() const -> QVariantList;

  // Moves a card into `column_id` and, if `swimlane_id` names an epic, reassigns the card's
  // `parent` to that epic (clearing `parent` when `swimlane_id` is kNoEpicSwimlaneId). This is
  // the one mutation the QML board performs; both cross-column and cross-swimlane drags funnel
  // through it.
  Q_INVOKABLE void moveCard(const QString& task_id, const QString& column_id,
                            const QString& swimlane_id);

  // Called from KanbanCard.qml's double-click gesture; just re-emits editTaskRequested so
  // KanbanBoardWidget (C++) can open a native edit dialog — QML has no business owning dialog UI.
  Q_INVOKABLE void requestEditTask(const QString& task_id);

  // Appends a new status column with a fresh, board-unique id derived from `label`.
  void addColumn(const QString& label);

  // Appends a new, unparented task with a fresh id into `column_id`. `is_epic` sets the task's
  // type to "summary" (see KanbanTask::IsEpic()), turning it into its own swimlane instead of a
  // regular card -- the native "Add epic" entry point (see KanbanBoardWidget) funnels here with
  // is_epic == true, "Add task" with is_epic == false; both share this one method since an epic
  // is just a task with a different type, not a different model. `start`/`duration` are the same
  // fields the Table view edits (KanbanTask::start/duration) -- without them, a task created here
  // has no schedule at all, which used to leave it with no visible bar in the Gantt view.
  void addTask(const QString& text, const QString& column_id, int priority, int progress,
               bool is_epic, const QString& description, const QStringList& tags,
               const QStringList& users, const QString& start, double duration);

  // Updates an existing task's Kanban-editable fields in place; a no-op if `task_id` doesn't
  // match any task currently on the board.
  void updateTask(const QString& task_id, const QString& text, const QString& column_id,
                  int priority, int progress, bool is_epic, const QString& description,
                  const QStringList& tags, const QStringList& users, const QString& start,
                  double duration);

  [[nodiscard]] auto FindTask(const QString& task_id) const -> std::optional<KanbanTask>;
  // The board's first status column id, or an empty string if the board has none — used to give
  // a newly-created task a sensible default status.
  [[nodiscard]] auto FirstColumnId() const -> QString;

 signals:
  void boardChanged();
  void editTaskRequested(const QString& task_id);

 private:
  [[nodiscard]] auto IsKnownEpicId(const QString& task_id) const -> bool;

  KanbanBoardDocument document_;
};

}  // namespace cppwiki::gui::kanban

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_BOARD_MODEL_H_
