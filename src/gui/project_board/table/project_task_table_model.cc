#include "gui/project_board/table/project_task_table_model.h"

#include <algorithm>
#include <numeric>

namespace cppwiki::gui::project_board {

namespace {

// Matches GridTab's `columnToneById` (project-board-pill--tone-0..5 in styles.css): six tones,
// cycling by the status column's position in the board's own column list.
constexpr int kStatusToneCount = 6;

}  // namespace

ProjectTaskTableModel::ProjectTaskTableModel(QObject* parent) : QAbstractTableModel(parent) {}

int ProjectTaskTableModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(tasks_.size());
}

int ProjectTaskTableModel::columnCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return kColumnCount;
}

QString ProjectTaskTableModel::statusLabelForColumnId(const QString& column_id) const {
  for (const ProjectColumn& column : board_columns_) {
    if (column.id == column_id) {
      return column.label;
    }
  }
  return QStringLiteral("Unassigned");
}

int ProjectTaskTableModel::statusToneForColumnId(const QString& column_id) const {
  for (int i = 0; i < board_columns_.size(); ++i) {
    if (board_columns_.at(i).id == column_id) {
      return i % kStatusToneCount;
    }
  }
  return -1;
}

QVariant ProjectTaskTableModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= tasks_.size()) {
    return QVariant();
  }
  const ProjectTask& task = tasks_.at(index.row());

  switch (index.column()) {
    case kTaskColumn:
      if (role == Qt::DisplayRole || role == Qt::EditRole) {
        return task.text();
      }
      break;
    case kStatusColumn:
      if (role == Qt::DisplayRole || role == kPillLabelRole) {
        return statusLabelForColumnId(task.column());
      }
      if (role == Qt::EditRole) {
        return task.column();
      }
      if (role == kToneRole) {
        return statusToneForColumnId(task.column());
      }
      break;
    case kPriorityColumn:
      if (role == Qt::DisplayRole || role == kPillLabelRole) {
        return task.hasPriority() ? PriorityLabel(task.priority()) : QString();
      }
      if (role == Qt::EditRole) {
        return task.hasPriority() ? task.priority() : 0;
      }
      if (role == kToneRole) {
        return task.hasPriority() ? task.priority() : -1;
      }
      break;
    case kStartColumn:
      if (role == Qt::DisplayRole) {
        // "MMM d, yyyy" (e.g. "Jul 24, 2026") — Grid's default is the raw Date's verbose
        // toString(), which GridTab already works around client-side with the equivalent
        // toLocaleDateString() formatting; this mirrors that here.
        return task.start().isValid() ? task.start().toString(QStringLiteral("MMM d, yyyy"))
                                      : QString();
      }
      if (role == Qt::EditRole) {
        return task.start();
      }
      break;
    case kDurationColumn:
      if (role == Qt::DisplayRole || role == Qt::EditRole) {
        return task.duration();
      }
      if (role == Qt::TextAlignmentRole) {
        return QVariant(Qt::AlignCenter);
      }
      break;
    case kProgressColumn:
      if (role == Qt::DisplayRole) {
        return QStringLiteral("%1%").arg(task.progress());
      }
      if (role == Qt::EditRole) {
        return task.progress();
      }
      if (role == Qt::TextAlignmentRole) {
        return QVariant(Qt::AlignCenter);
      }
      break;
    default:
      break;
  }
  return QVariant();
}

bool ProjectTaskTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if (role != Qt::EditRole || !index.isValid() || index.row() < 0 || index.row() >= tasks_.size()) {
    return false;
  }

  ProjectTask task = tasks_.at(index.row());
  switch (index.column()) {
    case kTaskColumn:
      task.setText(value.toString());
      break;
    case kStatusColumn:
      task.setColumn(value.toString());
      break;
    case kPriorityColumn: {
      const int priority = value.toInt();
      if (priority == kPriorityLow || priority == kPriorityMedium || priority == kPriorityHigh) {
        task.setPriority(priority);
      } else {
        task.clearPriority();
      }
      break;
    }
    case kStartColumn: {
      const QDate date = value.toDate();
      if (!date.isValid()) {
        return false;
      }
      task.setStart(date);
      break;
    }
    case kDurationColumn:
      task.setDuration(std::max(0, value.toInt()));
      break;
    case kProgressColumn:
      task.setProgress(std::clamp(value.toInt(), 0, 100));
      break;
    default:
      return false;
  }

  tasks_[index.row()] = task;
  emit dataChanged(index, index, {role, Qt::DisplayRole, kToneRole, kPillLabelRole});
  return true;
}

QVariant ProjectTaskTableModel::headerData(int section, Qt::Orientation orientation,
                                           int role) const {
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
    return QAbstractTableModel::headerData(section, orientation, role);
  }
  switch (section) {
    case kTaskColumn:
      return QStringLiteral("Task");
    case kStatusColumn:
      return QStringLiteral("Status");
    case kPriorityColumn:
      return QStringLiteral("Priority");
    case kStartColumn:
      return QStringLiteral("Start");
    case kDurationColumn:
      return QStringLiteral("Duration (days)");
    case kProgressColumn:
      return QStringLiteral("Progress %");
    default:
      return QVariant();
  }
}

Qt::ItemFlags ProjectTaskTableModel::flags(const QModelIndex& index) const {
  if (!index.isValid()) {
    return Qt::NoItemFlags;
  }
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

void ProjectTaskTableModel::setTasks(QVector<ProjectTask> tasks) {
  beginResetModel();
  tasks_ = std::move(tasks);
  endResetModel();
}

void ProjectTaskTableModel::setBoardColumns(QVector<ProjectColumn> columns) {
  board_columns_ = std::move(columns);
  if (!tasks_.isEmpty()) {
    emit dataChanged(index(0, kStatusColumn), index(rowCount() - 1, kStatusColumn),
                     {Qt::DisplayRole, kToneRole, kPillLabelRole});
  }
}

bool ProjectTaskTableModel::lessThan(int left_row, int right_row, int column) const {
  const ProjectTask& left = tasks_.at(left_row);
  const ProjectTask& right = tasks_.at(right_row);

  switch (column) {
    case kTaskColumn:
      return left.text().compare(right.text(), Qt::CaseInsensitive) < 0;
    case kStatusColumn: {
      // Sort by the status's position in the board's own column list (its workflow order, e.g.
      // To do -> In progress -> Done) rather than alphabetically by id/label — a status further
      // along the workflow should sort after one earlier in it.
      auto rank = [this](const QString& column_id) {
        for (int i = 0; i < board_columns_.size(); ++i) {
          if (board_columns_.at(i).id == column_id) {
            return i;
          }
        }
        return static_cast<int>(board_columns_.size());
      };
      return rank(left.column()) < rank(right.column());
    }
    case kPriorityColumn: {
      const int left_priority = left.hasPriority() ? left.priority() : 0;
      const int right_priority = right.hasPriority() ? right.priority() : 0;
      return left_priority < right_priority;
    }
    case kStartColumn:
      return left.start() < right.start();
    case kDurationColumn:
      return left.duration() < right.duration();
    case kProgressColumn:
      return left.progress() < right.progress();
    default:
      return false;
  }
}

void ProjectTaskTableModel::sort(int column, Qt::SortOrder order) {
  if (tasks_.isEmpty() || column < 0 || column >= kColumnCount) {
    return;
  }

  emit layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);
  const QModelIndexList old_persistent = persistentIndexList();

  QVector<int> permutation(tasks_.size());
  std::iota(permutation.begin(), permutation.end(), 0);
  std::stable_sort(permutation.begin(), permutation.end(), [&](int left_row, int right_row) {
    return order == Qt::AscendingOrder ? lessThan(left_row, right_row, column)
                                       : lessThan(right_row, left_row, column);
  });

  QVector<ProjectTask> sorted_tasks;
  sorted_tasks.reserve(tasks_.size());
  QVector<int> new_row_of_old_row(tasks_.size());
  for (int new_row = 0; new_row < permutation.size(); ++new_row) {
    const int old_row = permutation.at(new_row);
    new_row_of_old_row[old_row] = new_row;
    sorted_tasks.append(tasks_.at(old_row));
  }
  tasks_ = std::move(sorted_tasks);

  QModelIndexList new_persistent;
  new_persistent.reserve(old_persistent.size());
  for (const QModelIndex& old_index : old_persistent) {
    if (!old_index.isValid()) {
      new_persistent.append(old_index);
      continue;
    }
    new_persistent.append(
        index(new_row_of_old_row.at(old_index.row()), old_index.column(), old_index.parent()));
  }
  changePersistentIndexList(old_persistent, new_persistent);

  emit layoutChanged({}, QAbstractItemModel::VerticalSortHint);
}

}  // namespace cppwiki::gui::project_board
