// Small standalone demo for ProjectTaskTableWidget (see project_task_table_widget.h) — not part of
// the app proper, just a way to visually exercise the widget outside of the (not-yet-wired-in)
// Project board document kind. Run it directly: ./cppwiki_project_board_table_demo
//
// It loads a handful of sample tasks/columns, shows the widget, and prints the serialized document
// to stdout on every edit so a round trip through ProjectTaskTableModel/ProjectTask can be watched
// live (click a Status/Priority cell to pick from the dropdown, double-click Start for the date
// picker, edit Duration/Progress via the spin box, or click a header to sort).
#include <QApplication>
#include <QDebug>

#include "gui/project_board/table/project_task.h"
#include "gui/project_board/table/project_task_table_widget.h"

namespace {

using cppwiki::gui::project_board::ParseProjectBoardJson;
using cppwiki::gui::project_board::ProjectTaskTableWidget;
using cppwiki::gui::project_board::SerializeProjectBoardJson;

constexpr auto kSampleBoardJson = R"json({
  "tasks": [
    {"id": "task-1", "text": "Design the schema", "start": "2026-07-20T00:00:00.000Z",
     "duration": 3, "progress": 100, "column": "done", "priority": 2},
    {"id": "task-2", "text": "Build the table model", "start": "2026-07-23T00:00:00.000Z",
     "duration": 4, "progress": 60, "column": "inProgress", "priority": 3,
     "tags": ["backend"], "users": ["artem"]},
    {"id": "task-3", "text": "Write delegates", "start": "2026-07-24T00:00:00.000Z",
     "duration": 2, "progress": 20, "column": "inProgress", "priority": 3},
    {"id": "task-4", "text": "Wire up sorting", "start": "2026-07-26T00:00:00.000Z",
     "duration": 1, "progress": 0, "column": "todo"},
    {"id": "task-5", "text": "Ship the demo", "start": "2026-07-29T00:00:00.000Z",
     "duration": 1, "progress": 0, "column": "todo", "priority": 1}
  ],
  "columns": [
    {"id": "todo", "label": "To do"},
    {"id": "inProgress", "label": "In progress"},
    {"id": "done", "label": "Done"}
  ],
  "links": []
})json";

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);

  const auto document = ParseProjectBoardJson(QString::fromUtf8(kSampleBoardJson));
  if (!document.has_value()) {
    qCritical() << "Failed to parse the sample project board JSON";
    return 1;
  }

  ProjectTaskTableWidget widget;
  widget.setDocument(*document);
  widget.setWindowTitle(QStringLiteral("Project board — Table (native QTableView demo)"));
  widget.resize(900, 400);
  widget.show();

  QObject::connect(&widget, &ProjectTaskTableWidget::documentEdited, [&widget]() {
    qInfo().noquote() << "--- document after edit ---";
    qInfo().noquote() << SerializeProjectBoardJson(widget.document());
  });

  return QApplication::exec();
}
