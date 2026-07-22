// Standalone demo for ProjectBoardGanttWidget (issue #113/#119) — not part of cppwiki_app.
// Loads a small sample board (a "Design" summary task with two subtasks, a milestone, and a
// finish-to-start dependency link) and shows the widget so the KDGantt-backed rendering,
// drag/resize editing, and dependency-link drawing can be exercised by hand. See the PR/issue
// comments for how to run it and what to look for.

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

#include "gui/project_board/gantt/project_board_gantt_widget.h"

namespace {

auto MakeSampleBoard() -> QJsonObject {
  const auto today = QDateTime::currentDateTimeUtc();
  const auto ForDaysFromNow = [&today](int days) {
    return today.addDays(days).toString(Qt::ISODateWithMs);
  };

  QJsonObject design_task;
  design_task.insert("id", "task-design");
  design_task.insert("text", "Design");
  design_task.insert("start", ForDaysFromNow(0));
  design_task.insert("end", ForDaysFromNow(5));
  design_task.insert("duration", 5);
  design_task.insert("progress", 40);
  design_task.insert("column", "inProgress");
  design_task.insert("type", "summary");

  QJsonObject wireframes_task;
  wireframes_task.insert("id", "task-wireframes");
  wireframes_task.insert("text", "Wireframes");
  wireframes_task.insert("start", ForDaysFromNow(0));
  wireframes_task.insert("end", ForDaysFromNow(2));
  wireframes_task.insert("duration", 2);
  wireframes_task.insert("progress", 100);
  wireframes_task.insert("column", "done");
  wireframes_task.insert("parent", "task-design");
  wireframes_task.insert("type", "task");

  QJsonObject mockups_task;
  mockups_task.insert("id", "task-mockups");
  mockups_task.insert("text", "High-fidelity mockups");
  mockups_task.insert("start", ForDaysFromNow(2));
  mockups_task.insert("end", ForDaysFromNow(5));
  mockups_task.insert("duration", 3);
  mockups_task.insert("progress", 10);
  mockups_task.insert("column", "inProgress");
  mockups_task.insert("parent", "task-design");
  mockups_task.insert("type", "task");
  mockups_task.insert("tags", QJsonArray{"design"});
  mockups_task.insert("users", QJsonArray{"artem"});
  mockups_task.insert("priority", 3);

  QJsonObject review_milestone;
  review_milestone.insert("id", "milestone-review");
  review_milestone.insert("text", "Design review");
  review_milestone.insert("start", ForDaysFromNow(5));
  review_milestone.insert("end", ForDaysFromNow(5));
  review_milestone.insert("duration", 0);
  review_milestone.insert("progress", 0);
  review_milestone.insert("column", "todo");
  review_milestone.insert("type", "milestone");

  QJsonArray tasks{design_task, wireframes_task, mockups_task, review_milestone};

  QJsonArray columns{QJsonObject{{"id", "todo"}, {"label", "To do"}},
                     QJsonObject{{"id", "inProgress"}, {"label", "In progress"}},
                     QJsonObject{{"id", "done"}, {"label", "Done"}}};

  QJsonObject link;
  link.insert("id", "link-mockups-review");
  link.insert("type", "e2s");
  link.insert("source", "task-mockups");
  link.insert("target", "milestone-review");
  QJsonArray links{link};

  QJsonObject board;
  board.insert("tasks", tasks);
  board.insert("columns", columns);
  board.insert("links", links);
  return board;
}

}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);

  cppwiki::gui::project_board::gantt::ProjectBoardGanttWidget widget;
  widget.setWindowTitle("CppWiki Project board — native Gantt demo (KDGantt)");
  widget.resize(1000, 500);
  widget.LoadFromJson(MakeSampleBoard());

  QObject::connect(
      &widget, &cppwiki::gui::project_board::gantt::ProjectBoardGanttWidget::DataChanged,
      [](const QJsonObject& board) {
        qInfo() << "Board edited, task count:" << board.value("tasks").toArray().size();
      });

  widget.show();
  return QApplication::exec();
}
