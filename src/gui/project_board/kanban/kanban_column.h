#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_COLUMN_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_COLUMN_H_

#include <QJsonObject>
#include <QString>

namespace cppwiki::gui::kanban {

// Mirrors `ProjectColumn` from frontend/editor/src/project/projectBoard.ts.
struct KanbanColumn {
  QString id;
  QString label;

  [[nodiscard]] static auto FromJson(const QJsonObject& obj) -> KanbanColumn;
  [[nodiscard]] auto ToJson() const -> QJsonObject;
};

}  // namespace cppwiki::gui::kanban

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_COLUMN_H_
