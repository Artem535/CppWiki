#include <KDGanttConstraint>
#include <KDGanttConstraintModel>
#include <KDGanttGlobal>
#include <QApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <cstdlib>
#include <iostream>

#include "gui/project_board/gantt/project_board_gantt_critical_path.h"
#include "gui/project_board/gantt/project_board_gantt_model.h"
#include "gui/project_board/gantt/project_board_gantt_widget.h"

namespace {

using cppwiki::gui::project_board::gantt::ComputeCriticalPath;
using cppwiki::gui::project_board::gantt::ProjectBoardGanttModel;
using cppwiki::gui::project_board::gantt::ProjectBoardGanttWidget;

void Require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

QJsonObject SampleBoard() {
  QJsonObject design_task;
  design_task.insert("id", "task-design");
  design_task.insert("text", "Design");
  design_task.insert("start", "2026-01-01T00:00:00.000Z");
  design_task.insert("end", "2026-01-06T00:00:00.000Z");
  design_task.insert("duration", 5);
  design_task.insert("progress", 40);
  design_task.insert("column", "inProgress");
  design_task.insert("type", "summary");

  QJsonObject wireframes_task;
  wireframes_task.insert("id", "task-wireframes");
  wireframes_task.insert("text", "Wireframes");
  wireframes_task.insert("start", "2026-01-01T00:00:00.000Z");
  wireframes_task.insert("end", "2026-01-03T00:00:00.000Z");
  wireframes_task.insert("duration", 2);
  wireframes_task.insert("progress", 100);
  wireframes_task.insert("column", "done");
  wireframes_task.insert("parent", "task-design");
  wireframes_task.insert("type", "task");
  wireframes_task.insert("tags", QJsonArray{"design"});
  wireframes_task.insert("users", QJsonArray{"artem"});
  wireframes_task.insert("priority", 2);

  QJsonObject milestone_task;
  milestone_task.insert("id", "milestone-review");
  milestone_task.insert("text", "Design review");
  milestone_task.insert("start", "2026-01-06T00:00:00.000Z");
  milestone_task.insert("end", "2026-01-06T00:00:00.000Z");
  milestone_task.insert("duration", 0);
  milestone_task.insert("progress", 0);
  milestone_task.insert("column", "todo");
  milestone_task.insert("type", "milestone");

  QJsonArray tasks{design_task, wireframes_task, milestone_task};
  QJsonArray columns{QJsonObject{{"id", "todo"}, {"label", "To do"}},
                     QJsonObject{{"id", "inProgress"}, {"label", "In progress"}},
                     QJsonObject{{"id", "done"}, {"label", "Done"}}};

  QJsonObject link;
  link.insert("id", "link-wireframes-review");
  link.insert("type", "e2s");
  link.insert("source", "task-wireframes");
  link.insert("target", "milestone-review");
  QJsonArray links{link};

  QJsonObject board;
  board.insert("tasks", tasks);
  board.insert("columns", columns);
  board.insert("links", links);
  return board;
}

void TestLoadFromJsonRoundTripsTasksColumnsAndLinks() {
  ProjectBoardGanttWidget widget;
  widget.LoadFromJson(SampleBoard());

  const auto board = widget.ToJson();
  const auto tasks = board.value("tasks").toArray();
  Require(tasks.size() == 3, "expected 3 tasks after round-trip");

  const auto columns = board.value("columns").toArray();
  Require(columns.size() == 3, "columns should be passed through verbatim");

  bool found_wireframes = false;
  for (const auto& entry : tasks) {
    const auto task = entry.toObject();
    if (task.value("id").toString() != "task-wireframes") {
      continue;
    }
    found_wireframes = true;
    Require(task.value("parent").toString() == "task-design",
            "wireframes task should keep its parent (task-design) after round-trip");
    Require(task.value("type").toString() == "task", "wireframes task type should be 'task'");
    Require(task.value("tags").toArray().size() == 1, "wireframes tags should round-trip");
    Require(task.value("priority").toInt() == 2, "wireframes priority should round-trip");
  }
  Require(found_wireframes, "task-wireframes should be present after round-trip");

  const auto links = board.value("links").toArray();
  Require(links.size() == 1, "expected 1 dependency link after round-trip");
  const auto link = links.at(0).toObject();
  Require(link.value("type").toString() == "e2s", "link type should round-trip as e2s");
  Require(link.value("source").toString() == "task-wireframes", "link source should round-trip");
  Require(link.value("target").toString() == "milestone-review", "link target should round-trip");
}

void TestLoadFromJsonBuildsSummaryHierarchyForKdGantt() {
  ProjectBoardGanttModel model;
  model.LoadFromJson(SampleBoard());

  Require(model.rowCount() == 2, "expected 2 top-level rows (design summary + milestone)");

  const auto design_index = model.IndexForTaskId("task-design");
  Require(design_index.isValid(), "task-design should be found by id");
  Require(static_cast<KDGantt::ItemType>(model.data(design_index, KDGantt::ItemTypeRole).toInt()) ==
              KDGantt::TypeSummary,
          "task-design should map to KDGantt::TypeSummary");
  Require(model.itemFromIndex(design_index)->rowCount() == 1,
          "task-design should have exactly one child row (task-wireframes)");

  const auto milestone_index = model.IndexForTaskId("milestone-review");
  Require(milestone_index.isValid(), "milestone-review should be found by id");
  Require(static_cast<KDGantt::ItemType>(
              model.data(milestone_index, KDGantt::ItemTypeRole).toInt()) == KDGantt::TypeEvent,
          "milestone-review should map to KDGantt::TypeEvent");
}

void TestLoadFromJsonDoesNotEmitDataChanged() {
  ProjectBoardGanttWidget widget;
  int emit_count = 0;
  QObject::connect(&widget, &ProjectBoardGanttWidget::DataChanged,
                   [&emit_count](const QJsonObject&) { ++emit_count; });

  widget.LoadFromJson(SampleBoard());

  Require(emit_count == 0, "LoadFromJson() should not itself emit DataChanged (that's for edits)");
}

void TestDraggingATaskBarEmitsDataChangedWithUpdatedTask() {
  ProjectBoardGanttWidget widget;
  widget.LoadFromJson(SampleBoard());

  QJsonObject last_board;
  int emit_count = 0;
  QObject::connect(&widget, &ProjectBoardGanttWidget::DataChanged,
                   [&emit_count, &last_board](const QJsonObject& board) {
                     ++emit_count;
                     last_board = board;
                   });

  // Simulate what KDGantt::View's GraphicsView does internally when the user drags a bar: it
  // calls QAbstractItemModel::setData() with StartTimeRole/EndTimeRole on the model it was given.
  auto* model = widget.Model();
  const auto index = model->IndexForTaskId("task-wireframes");
  Require(index.isValid(), "task-wireframes should be found by id");

  const auto new_start = QDateTime::fromString("2026-01-02T00:00:00.000Z", Qt::ISODateWithMs);
  model->setData(index, new_start, KDGantt::StartTimeRole);

  // "task-wireframes" is a child of the "Design" summary task, so KDGantt's internal
  // SummaryHandlingProxyModel legitimately recomputes and writes back the summary's own
  // start/end span in response -- a second, cascading DataChanged for that recompute is
  // correct behavior (see issue #119's proxy column-mapping fix), not a double-emit bug.
  Require(emit_count >= 1, "a task edit should emit at least one DataChanged");
  const auto tasks = last_board.value("tasks").toArray();
  bool found = false;
  for (const auto& entry : tasks) {
    const auto task = entry.toObject();
    if (task.value("id").toString() != "task-wireframes") {
      continue;
    }
    found = true;
    Require(task.value("start").toString() == "2026-01-02T00:00:00.000Z",
            "DataChanged's payload should reflect the edited start time");
  }
  Require(found, "task-wireframes should still be present in the DataChanged payload");
}

void TestAddingADependencyLinkEmitsDataChanged() {
  ProjectBoardGanttWidget widget;
  widget.LoadFromJson(SampleBoard());

  int emit_count = 0;
  QObject::connect(&widget, &ProjectBoardGanttWidget::DataChanged,
                   [&emit_count](const QJsonObject&) { ++emit_count; });

  auto* model = widget.Model();
  const auto design_index = model->IndexForTaskId("task-design");
  const auto milestone_index = model->IndexForTaskId("milestone-review");
  Require(design_index.isValid() && milestone_index.isValid(),
          "both endpoints of the new link should be found by id");

  model->ConstraintModel()->addConstraint(KDGantt::Constraint(design_index, milestone_index,
                                                              KDGantt::Constraint::TypeHard,
                                                              KDGantt::Constraint::FinishStart));

  Require(emit_count == 1, "adding a dependency link should emit DataChanged exactly once");
  const auto links = widget.ToJson().value("links").toArray();
  Require(links.size() == 2, "expected 2 links after adding a new one");
}

QJsonObject MakeTask(const QString& id, int duration) {
  QJsonObject task;
  task.insert("id", id);
  task.insert("text", id);
  task.insert("duration", duration);
  task.insert("column", "todo");
  task.insert("type", "task");
  return task;
}

QJsonObject MakeLink(const QString& id, const QString& source, const QString& target) {
  QJsonObject link;
  link.insert("id", id);
  link.insert("type", "e2s");
  link.insert("source", source);
  link.insert("target", target);
  return link;
}

void TestCriticalPathFollowsLongestChain() {
  // Chain A->B->C totals 9 days; the independent D->E chain totals only 2 -- the critical path
  // should be exactly the longer chain, and the shorter one should have slack (not critical).
  QJsonObject board;
  board.insert("tasks", QJsonArray{MakeTask("a", 3), MakeTask("b", 2), MakeTask("c", 4),
                                   MakeTask("d", 1), MakeTask("e", 1)});
  board.insert("links", QJsonArray{MakeLink("l1", "a", "b"), MakeLink("l2", "b", "c"),
                                   MakeLink("l3", "d", "e")});

  const auto result = ComputeCriticalPath(board);
  Require(result.critical_task_ids == QSet<QString>({"a", "b", "c"}),
          "critical path should be exactly the longer a->b->c chain");
  Require(result.critical_link_ids == QSet<QString>({"l1", "l2"}),
          "critical links should be exactly the two links on the longer chain");
}

void TestCriticalPathEmptyOnCycle() {
  QJsonObject board;
  board.insert("tasks", QJsonArray{MakeTask("a", 1), MakeTask("b", 1)});
  board.insert("links", QJsonArray{MakeLink("l1", "a", "b"), MakeLink("l2", "b", "a")});

  const auto result = ComputeCriticalPath(board);
  Require(result.critical_task_ids.isEmpty() && result.critical_link_ids.isEmpty(),
          "a cycle makes CPM undefined -- expect no highlighting rather than a wrong answer");
}

void TestCriticalPathWithNoLinksPicksLongestTask() {
  QJsonObject board;
  board.insert("tasks", QJsonArray{MakeTask("short", 2), MakeTask("long", 5)});
  board.insert("links", QJsonArray{});

  const auto result = ComputeCriticalPath(board);
  Require(result.critical_task_ids == QSet<QString>({"long"}),
          "with no dependencies, only the single longest task should be critical");
}

}  // namespace

int main(int argc, char* argv[]) {
  qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
  QApplication application(argc, argv);

  TestLoadFromJsonRoundTripsTasksColumnsAndLinks();
  TestLoadFromJsonBuildsSummaryHierarchyForKdGantt();
  TestLoadFromJsonDoesNotEmitDataChanged();
  TestDraggingATaskBarEmitsDataChangedWithUpdatedTask();
  TestAddingADependencyLinkEmitsDataChanged();
  TestCriticalPathFollowsLongestChain();
  TestCriticalPathEmptyOnCycle();
  TestCriticalPathWithNoLinksPicksLongestTask();

  return EXIT_SUCCESS;
}
