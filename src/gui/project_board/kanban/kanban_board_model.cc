#include "gui/project_board/kanban/kanban_board_model.h"

#include <QSet>
#include <QVariantMap>
#include <QVector>
#include <utility>

namespace cppwiki::gui::kanban {

namespace {

struct SwimlaneInfo {
  QString id;
  QString label;
};

}  // namespace

KanbanBoardModel::KanbanBoardModel(QObject* parent) : QObject(parent) {}

void KanbanBoardModel::SetDocument(KanbanBoardDocument document) {
  document_ = std::move(document);
  emit boardChanged();
}

auto KanbanBoardModel::ExportDocument() const -> KanbanBoardDocument {
  return document_;
}

auto KanbanBoardModel::columns() const -> QVariantList {
  QVariantList result;
  for (const auto& column : document_.columns) {
    QVariantMap entry;
    entry.insert(QStringLiteral("id"), column.id);
    entry.insert(QStringLiteral("label"), column.label);
    result.append(entry);
  }
  return result;
}

auto KanbanBoardModel::rows() const -> QVariantList {
  // Epics in order of first appearance become swimlanes; a trailing "No epic" swimlane always
  // follows so cards without (or with an unmatched) parent still have somewhere to render and
  // somewhere to be dragged to.
  QVector<SwimlaneInfo> swimlanes;
  QSet<QString> epic_ids;
  for (const auto& task : document_.tasks) {
    if (task.IsEpic() && !epic_ids.contains(task.id)) {
      epic_ids.insert(task.id);
      swimlanes.append({task.id, task.text.isEmpty() ? task.id : task.text});
    }
  }
  swimlanes.append({QString(QLatin1String(kNoEpicSwimlaneId)), QStringLiteral("No epic")});

  QVariantList rows_result;
  for (const auto& lane : swimlanes) {
    QVariantList columns_result;
    for (const auto& column : document_.columns) {
      QVariantList cards;
      for (const auto& task : document_.tasks) {
        if (task.IsEpic() || task.column != column.id) {
          continue;
        }
        const QString task_lane =
            (!task.parent.isEmpty() && epic_ids.contains(task.parent)) ? task.parent : QString();
        if (task_lane != lane.id) {
          continue;
        }
        QVariantMap card;
        card.insert(QStringLiteral("id"), task.id);
        card.insert(QStringLiteral("text"), task.text);
        card.insert(QStringLiteral("priority"), task.priority);
        card.insert(QStringLiteral("progress"), task.progress);
        card.insert(QStringLiteral("column"), task.column);
        card.insert(QStringLiteral("parent"), task.parent);
        cards.append(card);
      }
      QVariantMap column_entry;
      column_entry.insert(QStringLiteral("columnId"), column.id);
      column_entry.insert(QStringLiteral("cards"), cards);
      columns_result.append(column_entry);
    }

    QVariantMap lane_entry;
    lane_entry.insert(QStringLiteral("swimlaneId"), lane.id);
    lane_entry.insert(QStringLiteral("swimlaneLabel"), lane.label);
    lane_entry.insert(QStringLiteral("columns"), columns_result);
    rows_result.append(lane_entry);
  }
  return rows_result;
}

void KanbanBoardModel::moveCard(const QString& task_id, const QString& column_id,
                                const QString& swimlane_id) {
  for (auto& task : document_.tasks) {
    if (task.id != task_id) {
      continue;
    }
    task.column = column_id;
    if (swimlane_id.isEmpty()) {
      task.parent.clear();
    } else if (IsKnownEpicId(swimlane_id)) {
      task.parent = swimlane_id;
    }
    emit boardChanged();
    return;
  }
}

auto KanbanBoardModel::IsKnownEpicId(const QString& task_id) const -> bool {
  for (const auto& task : document_.tasks) {
    if (task.IsEpic() && task.id == task_id) {
      return true;
    }
  }
  return false;
}

}  // namespace cppwiki::gui::kanban
