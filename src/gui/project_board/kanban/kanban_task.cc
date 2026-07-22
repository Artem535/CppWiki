#include "gui/project_board/kanban/kanban_task.h"

#include <QJsonArray>
#include <QJsonValue>

namespace cppwiki::gui::kanban {

namespace {

auto ToStringList(const QJsonValue& value) -> QStringList {
  QStringList result;
  if (!value.isArray()) {
    return result;
  }
  const auto array = value.toArray();
  result.reserve(array.size());
  for (const auto& entry : array) {
    result.append(entry.toString());
  }
  return result;
}

auto FromStringList(const QStringList& values) -> QJsonArray {
  QJsonArray array;
  for (const auto& value : values) {
    array.append(value);
  }
  return array;
}

}  // namespace

auto PriorityLabel(int priority) -> QString {
  switch (priority) {
    case kPriorityLow:
      return QStringLiteral("Low");
    case kPriorityMedium:
      return QStringLiteral("Medium");
    case kPriorityHigh:
      return QStringLiteral("High");
    default:
      return QString();
  }
}

auto KanbanTask::FromJson(const QJsonObject& obj) -> KanbanTask {
  KanbanTask task;
  task.id = obj.value(QLatin1String("id")).toString();
  task.text = obj.value(QLatin1String("text")).toString();
  task.start = obj.value(QLatin1String("start")).toString();
  task.duration = obj.value(QLatin1String("duration")).toDouble();
  task.end = obj.value(QLatin1String("end")).toString();
  task.progress = obj.value(QLatin1String("progress")).toDouble();
  task.column = obj.value(QLatin1String("column")).toString();
  task.parent = obj.value(QLatin1String("parent")).toString();
  task.type = obj.value(QLatin1String("type")).toString();
  task.tags = ToStringList(obj.value(QLatin1String("tags")));
  task.users = ToStringList(obj.value(QLatin1String("users")));
  task.priority = obj.value(QLatin1String("priority")).toInt();
  task.description = obj.value(QLatin1String("description")).toString();
  task.deadline = obj.value(QLatin1String("deadline")).toString();
  return task;
}

auto KanbanTask::ToJson() const -> QJsonObject {
  QJsonObject obj;
  obj.insert(QLatin1String("id"), id);
  obj.insert(QLatin1String("text"), text);
  obj.insert(QLatin1String("start"), start);
  obj.insert(QLatin1String("duration"), duration);
  if (!end.isEmpty()) {
    obj.insert(QLatin1String("end"), end);
  }
  obj.insert(QLatin1String("progress"), progress);
  obj.insert(QLatin1String("column"), column);
  if (!parent.isEmpty()) {
    obj.insert(QLatin1String("parent"), parent);
  }
  if (!type.isEmpty()) {
    obj.insert(QLatin1String("type"), type);
  }
  if (!tags.isEmpty()) {
    obj.insert(QLatin1String("tags"), FromStringList(tags));
  }
  if (!users.isEmpty()) {
    obj.insert(QLatin1String("users"), FromStringList(users));
  }
  if (priority != 0) {
    obj.insert(QLatin1String("priority"), priority);
  }
  if (!description.isEmpty()) {
    obj.insert(QLatin1String("description"), description);
  }
  if (!deadline.isEmpty()) {
    obj.insert(QLatin1String("deadline"), deadline);
  }
  return obj;
}

}  // namespace cppwiki::gui::kanban
