#include "gui/project_board/kanban/kanban_board_document.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace cppwiki::gui::kanban {

auto KanbanBoardDocument::ParseJson(const QByteArray& raw) -> std::optional<KanbanBoardDocument> {
  QJsonParseError parse_error;
  const auto json = QJsonDocument::fromJson(raw, &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !json.isObject()) {
    return std::nullopt;
  }

  const auto root = json.object();
  const auto tasks_value = root.value(QLatin1String("tasks"));
  const auto columns_value = root.value(QLatin1String("columns"));
  if (!tasks_value.isArray() || !columns_value.isArray()) {
    return std::nullopt;
  }

  KanbanBoardDocument document;
  const auto tasks_array = tasks_value.toArray();
  document.tasks.reserve(tasks_array.size());
  for (const auto& entry : tasks_array) {
    if (entry.isObject()) {
      document.tasks.append(KanbanTask::FromJson(entry.toObject()));
    }
  }

  const auto columns_array = columns_value.toArray();
  document.columns.reserve(columns_array.size());
  for (const auto& entry : columns_array) {
    if (entry.isObject()) {
      document.columns.append(KanbanColumn::FromJson(entry.toObject()));
    }
  }

  return document;
}

auto KanbanBoardDocument::ToJson() const -> QByteArray {
  QJsonArray tasks_array;
  for (const auto& task : tasks) {
    tasks_array.append(task.ToJson());
  }

  QJsonArray columns_array;
  for (const auto& column : columns) {
    columns_array.append(column.ToJson());
  }

  QJsonObject root;
  root.insert(QLatin1String("tasks"), tasks_array);
  root.insert(QLatin1String("columns"), columns_array);

  return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

}  // namespace cppwiki::gui::kanban
