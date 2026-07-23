#include "gui/project_board/gantt/project_board_gantt_model.h"

#include <KDGanttConstraint>
#include <KDGanttConstraintModel>
#include <KDGanttGlobal>
#include <QColor>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QPen>
#include <QStringList>
#include <QVariant>
#include <QtGlobal>
#include <cmath>
#include <functional>

namespace cppwiki::gui::project_board::gantt {

namespace {

// Constraint::setData()/data() roles used to preserve ProjectLink::id and ProjectLink::lag
// across a load/edit/save round trip — KDGantt::Constraint has its own small data-map namespace
// (separate from QAbstractItemModel roles), so these only need to avoid Constraint's own
// ValidConstraintPen/InvalidConstraintPen (Qt::UserRole, Qt::UserRole + 1).
constexpr int kLinkIdDataRole = Qt::UserRole + 500;
constexpr int kLinkLagDataRole = Qt::UserRole + 501;

auto ParseIsoDateTime(const QString& text) -> QDateTime {
  auto parsed = QDateTime::fromString(text, Qt::ISODateWithMs);
  if (!parsed.isValid()) {
    parsed = QDateTime::fromString(text, Qt::ISODate);
  }
  return parsed;
}

auto ToIsoString(const QDateTime& value) -> QString {
  return value.toUTC().toString(Qt::ISODateWithMs);
}

auto ItemTypeFromString(const QString& type) -> KDGantt::ItemType {
  if (type == QStringLiteral("summary")) {
    return KDGantt::TypeSummary;
  }
  if (type == QStringLiteral("milestone")) {
    return KDGantt::TypeEvent;
  }
  return KDGantt::TypeTask;
}

auto ItemTypeToString(KDGantt::ItemType type) -> QString {
  switch (type) {
    case KDGantt::TypeSummary:
      return QStringLiteral("summary");
    case KDGantt::TypeEvent:
      return QStringLiteral("milestone");
    default:
      return QStringLiteral("task");
  }
}

auto RelationTypeFromString(const QString& type) -> KDGantt::Constraint::RelationType {
  if (type == QStringLiteral("s2s")) {
    return KDGantt::Constraint::StartStart;
  }
  if (type == QStringLiteral("s2e")) {
    return KDGantt::Constraint::StartFinish;
  }
  if (type == QStringLiteral("e2e")) {
    return KDGantt::Constraint::FinishFinish;
  }
  return KDGantt::Constraint::FinishStart;
}

auto RelationTypeToString(KDGantt::Constraint::RelationType type) -> QString {
  switch (type) {
    case KDGantt::Constraint::StartStart:
      return QStringLiteral("s2s");
    case KDGantt::Constraint::StartFinish:
      return QStringLiteral("s2e");
    case KDGantt::Constraint::FinishFinish:
      return QStringLiteral("e2e");
    default:
      return QStringLiteral("e2s");
  }
}

auto ToStringList(const QJsonValue& value) -> QStringList {
  QStringList result;
  if (!value.isArray()) {
    return result;
  }
  for (const auto& entry : value.toArray()) {
    if (entry.isString()) {
      result.push_back(entry.toString());
    }
  }
  return result;
}

auto DurationInDays(const QDateTime& start, const QDateTime& end) -> int {
  if (!start.isValid() || !end.isValid()) {
    return 1;
  }
  const auto days = static_cast<double>(start.secsTo(end)) / 86400.0;
  const auto rounded = static_cast<int>(std::lround(days));
  return rounded > 0 ? rounded : 1;
}

}  // namespace

ProjectBoardGanttModel::ProjectBoardGanttModel(QObject* parent)
    : QStandardItemModel(parent), constraint_model_(std::make_unique<KDGantt::ConstraintModel>()) {
  setHorizontalHeaderLabels({QStringLiteral("Task")});
}

ProjectBoardGanttModel::~ProjectBoardGanttModel() = default;

void ProjectBoardGanttModel::LoadFromJson(const QJsonObject& board) {
  constraint_model_->clear();
  // QStandardItemModel::clear() wipes header labels along with every item, so the column-0
  // header set in the constructor has to be re-applied here too -- otherwise the left tree
  // panel's header falls back to QStandardItemModel::headerData()'s default of "section + 1"
  // ("1" for the only column), which is what happened before this fix.
  clear();
  setHorizontalHeaderLabels({QStringLiteral("Task")});

  columns_ = board.value(QStringLiteral("columns")).toArray();
  const auto tasks = board.value(QStringLiteral("tasks")).toArray();

  QHash<QString, QStandardItem*> items_by_id;
  QHash<QString, QString> parent_by_id;

  // Pass 1: create every row item (unattached) so forward references (a child listed before its
  // parent in `tasks`) resolve correctly in pass 2.
  for (const auto& entry : tasks) {
    if (!entry.isObject()) {
      continue;
    }
    const auto task = entry.toObject();
    const auto id = task.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) {
      continue;
    }

    auto* item = new QStandardItem(task.value(QStringLiteral("text")).toString());
    item->setEditable(true);

    auto start = ParseIsoDateTime(task.value(QStringLiteral("start")).toString());
    if (!start.isValid()) {
      // Tasks created from the Kanban view (KanbanBoardModel::addTask()) never set
      // start/end/duration at all -- a Kanban card doesn't need dates, but without a fallback
      // here the Gantt view would compute an invalid start/end and draw no bar whatsoever (see
      // paintGanttItem()'s `if (item_rect.isValid())` guard), silently hiding the task instead of
      // giving the user something visible and draggable to set a real schedule on.
      start = QDateTime::currentDateTimeUtc();
    }
    const auto has_explicit_end = task.contains(QStringLiteral("end"));
    const auto end = has_explicit_end
                         ? ParseIsoDateTime(task.value(QStringLiteral("end")).toString())
                         : start.addDays(qMax(1, task.value(QStringLiteral("duration")).toInt(1)));

    item->setData(
        ItemTypeFromString(task.value(QStringLiteral("type")).toString(QStringLiteral("task"))),
        KDGantt::ItemTypeRole);
    item->setData(start, KDGantt::StartTimeRole);
    item->setData(end.isValid() ? end : start, KDGantt::EndTimeRole);
    item->setData(task.value(QStringLiteral("progress")).toInt(0), KDGantt::TaskCompletionRole);

    item->setData(id, kTaskIdRole);
    item->setData(task.value(QStringLiteral("column")).toString(), kTaskColumnRole);
    item->setData(ToStringList(task.value(QStringLiteral("tags"))), kTaskTagsRole);
    item->setData(ToStringList(task.value(QStringLiteral("users"))), kTaskUsersRole);
    if (task.contains(QStringLiteral("priority"))) {
      item->setData(task.value(QStringLiteral("priority")).toInt(), kTaskPriorityRole);
    }
    if (task.contains(QStringLiteral("description"))) {
      item->setData(task.value(QStringLiteral("description")).toString(), kTaskDescriptionRole);
    }
    if (task.contains(QStringLiteral("deadline"))) {
      item->setData(task.value(QStringLiteral("deadline")).toString(), kTaskDeadlineRole);
    }

    items_by_id.insert(id, item);
    parent_by_id.insert(id, task.value(QStringLiteral("parent")).toString());
  }

  // Pass 2: group each item under its resolved parent (or the invisible root, keyed by
  // nullptr), preserving each task's relative order within its sibling group, then attach each
  // group in a single batched appendRows() call. KDGantt::GraphicsView rebuilds its entire scene
  // (clears every item, re-walks every row) on every rowsInserted signal it receives while
  // attached to a model (kdganttgraphicsview.cpp's slotRowsInserted() calls updateScene(),
  // tagged upstream with "TODO: This might be optimised"), so calling appendRow() once per task
  // made loading N tasks O(n^2) and was the dominant cause of the reported load-time lag (see
  // #119). Batching by parent means the already-attached view only rebuilds once per distinct
  // parent instead of once per task.
  QHash<QStandardItem*, QList<QStandardItem*>> children_by_parent;
  for (const auto& entry : tasks) {
    if (!entry.isObject()) {
      continue;
    }
    const auto id = entry.toObject().value(QStringLiteral("id")).toString();
    auto* item = items_by_id.value(id);
    if (item == nullptr) {
      continue;
    }

    const auto parent_id = parent_by_id.value(id);
    auto* parent_item = parent_id.isEmpty() ? nullptr : items_by_id.value(parent_id);
    children_by_parent[parent_item].push_back(item);
  }
  // Attach every non-root group first: each parent item is still floating (not yet part of the
  // model), so QStandardItem::appendRows() here doesn't touch the model and emits no signal at
  // all. Only the final invisibleRootItem()->appendRows() call below actually inserts anything
  // into the model -- and since the top-level items already have their whole descendant
  // subtrees built by that point, it brings the entire hierarchy in with exactly one
  // rowsInserted, regardless of how many distinct parents/levels exist.
  for (auto it = children_by_parent.constBegin(); it != children_by_parent.constEnd(); ++it) {
    if (it.key() != nullptr) {
      it.key()->appendRows(it.value());
    }
  }
  invisibleRootItem()->appendRows(children_by_parent.value(nullptr));

  const auto links = board.value(QStringLiteral("links")).toArray();
  for (const auto& entry : links) {
    if (!entry.isObject()) {
      continue;
    }
    const auto link = entry.toObject();
    const auto source_index = IndexForTaskId(link.value(QStringLiteral("source")).toString());
    const auto target_index = IndexForTaskId(link.value(QStringLiteral("target")).toString());
    if (!source_index.isValid() || !target_index.isValid()) {
      continue;
    }

    KDGantt::Constraint constraint(
        source_index, target_index, KDGantt::Constraint::TypeHard,
        RelationTypeFromString(link.value(QStringLiteral("type")).toString()));
    constraint.setData(kLinkIdDataRole, link.value(QStringLiteral("id")).toString());
    if (link.contains(QStringLiteral("lag"))) {
      constraint.setData(kLinkLagDataRole, link.value(QStringLiteral("lag")).toInt());
    }
    constraint.setData(KDGantt::Constraint::ValidConstraintPen,
                       QVariant::fromValue(ProjectBoardGanttModel::LinkPen()));
    constraint.setData(KDGantt::Constraint::InvalidConstraintPen,
                       QVariant::fromValue(ProjectBoardGanttModel::LinkPen()));
    constraint_model_->addConstraint(constraint);
  }
}

auto ProjectBoardGanttModel::ToJson() const -> QJsonObject {
  QJsonArray tasks;

  std::function<void(QStandardItem*, const QString&)> visit = [&](QStandardItem* item,
                                                                  const QString& parent_id) {
    QJsonObject task;
    task.insert(QStringLiteral("id"), item->data(kTaskIdRole).toString());
    task.insert(QStringLiteral("text"), item->data(Qt::DisplayRole).toString());

    const auto start = item->data(KDGantt::StartTimeRole).toDateTime();
    const auto end = item->data(KDGantt::EndTimeRole).toDateTime();
    task.insert(QStringLiteral("start"), ToIsoString(start));
    task.insert(QStringLiteral("end"), ToIsoString(end));
    task.insert(QStringLiteral("duration"), DurationInDays(start, end));
    task.insert(QStringLiteral("progress"), item->data(KDGantt::TaskCompletionRole).toInt());
    task.insert(QStringLiteral("column"), item->data(kTaskColumnRole).toString());
    task.insert(QStringLiteral("type"), ItemTypeToString(static_cast<KDGantt::ItemType>(
                                            item->data(KDGantt::ItemTypeRole).toInt())));
    if (!parent_id.isEmpty()) {
      task.insert(QStringLiteral("parent"), parent_id);
    }

    const auto tags = item->data(kTaskTagsRole).toStringList();
    if (!tags.isEmpty()) {
      task.insert(QStringLiteral("tags"), QJsonArray::fromStringList(tags));
    }
    const auto users = item->data(kTaskUsersRole).toStringList();
    if (!users.isEmpty()) {
      task.insert(QStringLiteral("users"), QJsonArray::fromStringList(users));
    }
    const auto priority = item->data(kTaskPriorityRole);
    if (priority.isValid()) {
      task.insert(QStringLiteral("priority"), priority.toInt());
    }
    const auto description = item->data(kTaskDescriptionRole);
    if (description.isValid()) {
      task.insert(QStringLiteral("description"), description.toString());
    }
    const auto deadline = item->data(kTaskDeadlineRole);
    if (deadline.isValid()) {
      task.insert(QStringLiteral("deadline"), deadline.toString());
    }

    tasks.push_back(task);

    const auto task_id = item->data(kTaskIdRole).toString();
    for (int row = 0; row < item->rowCount(); ++row) {
      visit(item->child(row), task_id);
    }
  };

  for (int row = 0; row < rowCount(); ++row) {
    visit(item(row), QString());
  }

  QJsonArray links;
  for (const auto& constraint : constraint_model_->constraints()) {
    const auto source_id = TaskIdForIndex(constraint.startIndex());
    const auto target_id = TaskIdForIndex(constraint.endIndex());
    if (source_id.isEmpty() || target_id.isEmpty()) {
      continue;
    }

    QJsonObject link;
    auto id = constraint.data(kLinkIdDataRole).toString();
    if (id.isEmpty()) {
      id = QStringLiteral("link-%1-%2").arg(source_id, target_id);
    }
    link.insert(QStringLiteral("id"), id);
    link.insert(QStringLiteral("type"), RelationTypeToString(constraint.relationType()));
    link.insert(QStringLiteral("source"), source_id);
    link.insert(QStringLiteral("target"), target_id);
    const auto lag = constraint.data(kLinkLagDataRole);
    if (lag.isValid()) {
      link.insert(QStringLiteral("lag"), lag.toInt());
    }
    links.push_back(link);
  }

  QJsonObject board;
  board.insert(QStringLiteral("tasks"), tasks);
  board.insert(QStringLiteral("columns"), columns_);
  board.insert(QStringLiteral("links"), links);
  return board;
}

auto ProjectBoardGanttModel::ConstraintModel() const -> KDGantt::ConstraintModel* {
  return constraint_model_.get();
}

auto ProjectBoardGanttModel::LinkPen() -> QPen {
  QPen pen(QColor(0xff, 0xff, 0xff, 0xe5));
  pen.setWidthF(2.0);
  return pen;
}

void ProjectBoardGanttModel::SetCriticalPathTaskIds(const QSet<QString>& critical_task_ids) {
  std::function<void(QStandardItem*)> visit = [&](QStandardItem* item) {
    const auto id = item->data(kTaskIdRole).toString();
    item->setData(critical_task_ids.contains(id), kTaskCriticalPathRole);
    for (int row = 0; row < item->rowCount(); ++row) {
      visit(item->child(row));
    }
  };
  for (int row = 0; row < rowCount(); ++row) {
    visit(item(row));
  }
}

auto ProjectBoardGanttModel::TaskIdForIndex(const QModelIndex& index) -> QString {
  if (!index.isValid()) {
    return {};
  }
  return index.data(kTaskIdRole).toString();
}

auto ProjectBoardGanttModel::IndexForTaskId(const QString& task_id) const -> QModelIndex {
  if (task_id.isEmpty() || rowCount() == 0) {
    return {};
  }
  const auto matches =
      match(index(0, 0), kTaskIdRole, task_id, 1, Qt::MatchExactly | Qt::MatchRecursive);
  return matches.isEmpty() ? QModelIndex() : matches.front();
}

}  // namespace cppwiki::gui::project_board::gantt
