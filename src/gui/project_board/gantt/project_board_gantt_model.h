#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_GANTT_PROJECT_BOARD_GANTT_MODEL_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_GANTT_PROJECT_BOARD_GANTT_MODEL_H_

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QString>
#include <memory>

namespace KDGantt {
class ConstraintModel;
}  // namespace KDGantt

namespace cppwiki::gui::project_board::gantt {

// Adapter between the web Project board's shared JSON task schema
// (frontend/editor/src/project/projectBoard.ts: ProjectTask/ProjectColumn/ProjectLink) and
// KDGantt::View's expected QAbstractItemModel shape. One row per task, single column; task
// hierarchy (ProjectTask::parent) becomes KDGantt summary/child rows via QStandardItem's own
// row nesting. Fields KDGantt doesn't render (column/tags/users/priority/description/deadline)
// are round-tripped losslessly through custom item-data roles so documents stay compatible with
// the web renderer even though this view never displays them.
class ProjectBoardGanttModel final : public QStandardItemModel {
  Q_OBJECT

 public:
  // Custom roles for fields KDGantt itself doesn't know about but that must survive a
  // load/edit/save round trip unchanged. Placed well above KDGantt's own
  // Qt::UserRole + 1174..1180 range (see KDGantt::ItemDataRole) to avoid collisions.
  enum Role {
    kTaskIdRole = Qt::UserRole + 2000,
    kTaskColumnRole,
    kTaskTagsRole,
    kTaskUsersRole,
    kTaskPriorityRole,
    kTaskDescriptionRole,
    kTaskDeadlineRole,
    // Transient, view-only: whether this task is on the critical path per the most recent
    // ComputeCriticalPath() call. Never round-tripped by ToJson() -- it's derived from the
    // current tasks/links each time the caller asks for it, not saved document state.
    kTaskCriticalPathRole,
  };

  explicit ProjectBoardGanttModel(QObject* parent = nullptr);
  ~ProjectBoardGanttModel() override;

  // Rebuilds the model (and the constraint/link model returned by ConstraintModel()) from a
  // `{ tasks, columns, links }` JSON object matching the shared ProjectBoard schema. Clears any
  // prior content first.
  void LoadFromJson(const QJsonObject& board);

  // Reconstructs the `{ tasks, columns, links }` JSON object from the current model and
  // constraint-model state. Always round-trips `columns` verbatim; `tasks`/`links` reflect any
  // edits (drag/resize/hierarchy/link changes) made since LoadFromJson().
  [[nodiscard]] auto ToJson() const -> QJsonObject;

  // Owned by this model; created once at construction and reused across LoadFromJson() calls so
  // callers (ProjectBoardGanttWidget) can wire it into KDGantt::View::setConstraintModel() once.
  [[nodiscard]] auto ConstraintModel() const -> KDGantt::ConstraintModel*;

  // Looks up the row index for a given ProjectTask::id, or an invalid QModelIndex if not found.
  // Exposed (rather than kept private) for tests and for future integration code that needs to
  // map an id from elsewhere (e.g. the Kanban/Table views, over the same document) to a row here.
  [[nodiscard]] auto IndexForTaskId(const QString& task_id) const -> QModelIndex;

  // Marks exactly the tasks in `critical_task_ids` with kTaskCriticalPathRole = true (every task
  // not in the set gets false), so ProjectBoardGanttItemDelegate can look the role up per bar.
  // Walks every row recursively (tasks can be nested under summary rows) each call rather than
  // only the previously/newly critical ones, since clearing a stale highlight is just as
  // important as setting a new one and the model has no cheap way to know which rows were
  // critical last time without keeping that bookkeeping itself.
  void SetCriticalPathTaskIds(const QSet<QString>& critical_task_ids);

 private:
  [[nodiscard]] static auto TaskIdForIndex(const QModelIndex& index) -> QString;

  QJsonArray columns_;
  std::unique_ptr<KDGantt::ConstraintModel> constraint_model_;
};

}  // namespace cppwiki::gui::project_board::gantt

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_GANTT_PROJECT_BOARD_GANTT_MODEL_H_
