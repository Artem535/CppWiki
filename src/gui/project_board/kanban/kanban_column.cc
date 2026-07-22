#include "gui/project_board/kanban/kanban_column.h"

namespace cppwiki::gui::kanban {

auto KanbanColumn::FromJson(const QJsonObject& obj) -> KanbanColumn {
  KanbanColumn column;
  column.id = obj.value(QLatin1String("id")).toString();
  column.label = obj.value(QLatin1String("label")).toString();
  return column;
}

auto KanbanColumn::ToJson() const -> QJsonObject {
  QJsonObject obj;
  obj.insert(QLatin1String("id"), id);
  obj.insert(QLatin1String("label"), label);
  return obj;
}

}  // namespace cppwiki::gui::kanban
