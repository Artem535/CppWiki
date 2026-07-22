#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_BOARD_DOCUMENT_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_BOARD_DOCUMENT_H_

#include <QByteArray>
#include <QVector>
#include <optional>

#include "gui/project_board/kanban/kanban_column.h"
#include "gui/project_board/kanban/kanban_task.h"

namespace cppwiki::gui::kanban {

// Mirrors `ProjectBoard` from frontend/editor/src/project/projectBoard.ts. `links` (Gantt-only
// dependency edges) are intentionally not modeled here — Kanban never reads or writes them, and
// this MVP doesn't round-trip the full document (see KanbanBoardDocument::ParseJson).
struct KanbanBoardDocument {
  QVector<KanbanTask> tasks;
  QVector<KanbanColumn> columns;

  // Parses a `{ tasks: [...], columns: [...] }` JSON document. Returns std::nullopt if `raw`
  // isn't valid JSON or is missing either top-level array.
  [[nodiscard]] static auto ParseJson(const QByteArray& raw) -> std::optional<KanbanBoardDocument>;

  // Serializes back to the same shape. Note: this MVP does not read `links`, so round-tripping a
  // document through this class drops any Gantt dependency links it had — fine for the standalone
  // demo, but a real integration would need to preserve the original `links` array untouched.
  [[nodiscard]] auto ToJson() const -> QByteArray;
};

}  // namespace cppwiki::gui::kanban

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_BOARD_DOCUMENT_H_
