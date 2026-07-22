// Standalone demo for the native Kanban board (issue: native-rendering follow-up to #113/#114).
// Not part of cppwiki_app — this just loads a small sample document (two epics, an unassigned
// task, three columns) into KanbanBoardWidget and shows it, so the component can be exercised
// without any app integration.
//
// Run with: ./cppwiki_kanban_board_demo
//
// Things to check manually:
//   - Drag a card between "To do" / "In progress" / "Done" within one epic's row (column move).
//   - Drag a card from one epic's row into another epic's row, or into "No epic" (swimlane move,
//     i.e. reassigning `parent`) — this is the capability the web/SVAR Kanban doesn't have.
//   - Close the window (prints the resulting JSON to stdout) and confirm `column`/`parent` on the
//     moved tasks reflect where they were dropped.

#include <QApplication>
#include <QByteArray>
#include <QDebug>
#include <QString>

#include "gui/project_board/kanban/kanban_board_widget.h"

namespace {

constexpr auto kSampleBoardJson = R"JSON(
{
  "columns": [
    { "id": "todo", "label": "To do" },
    { "id": "inProgress", "label": "In progress" },
    { "id": "done", "label": "Done" }
  ],
  "tasks": [
    { "id": "epic-auth", "text": "Authentik SSO rollout", "type": "summary",
      "start": "2026-07-01", "duration": 1, "progress": 40, "column": "inProgress" },
    { "id": "epic-sync", "text": "Sync conflict UX", "type": "summary",
      "start": "2026-07-01", "duration": 1, "progress": 10, "column": "todo" },

    { "id": "task-1", "text": "Wire OIDC login button", "parent": "epic-auth",
      "start": "2026-07-05", "duration": 3, "progress": 100, "column": "done", "priority": 2 },
    { "id": "task-2", "text": "Token refresh edge cases", "parent": "epic-auth",
      "start": "2026-07-08", "duration": 2, "progress": 50, "column": "inProgress", "priority": 3 },
    { "id": "task-3", "text": "Keychain storage on Windows", "parent": "epic-auth",
      "start": "2026-07-10", "duration": 2, "progress": 0, "column": "todo", "priority": 1 },

    { "id": "task-4", "text": "Design conflict merge dialog", "parent": "epic-sync",
      "start": "2026-07-03", "duration": 2, "progress": 100, "column": "done", "priority": 2 },
    { "id": "task-5", "text": "Three-way merge for block moves", "parent": "epic-sync",
      "start": "2026-07-06", "duration": 4, "progress": 20, "column": "todo", "priority": 3 },

    { "id": "task-6", "text": "Update README badges", "start": "2026-07-12", "duration": 1,
      "progress": 0, "column": "todo", "priority": 0 },
    { "id": "task-7", "text": "Bump vcpkg baseline", "start": "2026-07-13", "duration": 1,
      "progress": 0, "column": "inProgress", "priority": 1 }
  ]
}
)JSON";

}  // namespace

auto main(int argc, char** argv) -> int {
  QApplication app(argc, argv);

  cppwiki::gui::kanban::KanbanBoardWidget widget;
  widget.setWindowTitle(QStringLiteral("Native Kanban board (standalone demo)"));
  widget.LoadFromJson(QByteArray(kSampleBoardJson));
  widget.resize(900, 640);
  widget.show();

  const auto exit_code = QApplication::exec();

  qInfo().noquote() << "Final board state:\n" << widget.ToJson();

  return exit_code;
}
