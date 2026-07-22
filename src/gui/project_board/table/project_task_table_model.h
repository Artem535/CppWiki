#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_TASK_TABLE_MODEL_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_TASK_TABLE_MODEL_H_

#include <QAbstractTableModel>
#include <QVector>

#include "gui/project_board/table/project_task.h"

namespace cppwiki::gui::project_board {

// A flat QAbstractTableModel over one Project board's task list, matching the columns the web
// Table (Grid) view currently shows (see ProjectBoardView.tsx's GridTab): Task/Status/Priority/
// Start/Duration/Progress. Backed directly by ProjectTask/ProjectColumn (see project_task.h) so
// round-tripping through this model never touches fields this view doesn't render.
class ProjectTaskTableModel : public QAbstractTableModel {
  Q_OBJECT

 public:
  enum Column {
    kTaskColumn = 0,
    kStatusColumn,
    kPriorityColumn,
    kStartColumn,
    kDurationColumn,
    kProgressColumn,
    kColumnCount,
  };

  // Custom roles read by the pill delegates (see project_status_pill_delegate.h/
  // project_priority_pill_delegate.h) alongside the standard Display/Edit roles.
  enum Role {
    // int: for Status, the tone index (0..5, cycling) of the task's board column; for Priority,
    // the raw 1/2/3 priority value, or -1 if the task has no priority set.
    kToneRole = Qt::UserRole + 1,
    // QString: the human label to paint inside the pill (column label / priority name).
    kPillLabelRole,
  };

  explicit ProjectTaskTableModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  [[nodiscard]] int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
  [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                    int role = Qt::DisplayRole) const override;
  [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
  void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

  void setTasks(QVector<ProjectTask> tasks);
  [[nodiscard]] const QVector<ProjectTask>& tasks() const {
    return tasks_;
  }

  void setBoardColumns(QVector<ProjectColumn> columns);
  [[nodiscard]] const QVector<ProjectColumn>& boardColumns() const {
    return board_columns_;
  }

  [[nodiscard]] const ProjectTask& taskAt(int row) const {
    return tasks_.at(row);
  }

  // Label for a board status column id, or "Unassigned" if the task points at a column that no
  // longer exists (mirrors GridTab's StatusCell fallback for exactly the same situation).
  [[nodiscard]] QString statusLabelForColumnId(const QString& column_id) const;
  // Cycling tone index (0..5) for a board status column id by its position in the board's column
  // list, or -1 if unknown — mirrors GridTab's columnToneById.
  [[nodiscard]] int statusToneForColumnId(const QString& column_id) const;

 private:
  [[nodiscard]] bool lessThan(int left_row, int right_row, int column) const;

  QVector<ProjectTask> tasks_;
  QVector<ProjectColumn> board_columns_;
};

}  // namespace cppwiki::gui::project_board

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_TASK_TABLE_MODEL_H_
