#include <KDGanttConstraint>
#include <KDGanttConstraintModel>
#include <KDGanttGlobal>
#include <KDGanttGraphicsView>
#include <KDGanttView>
#include <QAbstractProxyModel>
#include <QApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QPen>
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

// KanbanBoardModel::addTask() (kanban_board_model.cc) creates a task with no start/end/duration
// at all -- fine for a Kanban card, but the same document is shared with this Gantt view, and
// without a fallback here LoadFromJson() would leave StartTimeRole/EndTimeRole invalid, which
// ProjectBoardGanttItemDelegate::paintGanttItem() (item_rect.isValid() guard) then draws as no
// bar whatsoever -- the task silently disappears from the Gantt view instead of being visible
// and draggable.
void TestLoadFromJsonDefaultsStartDateForTaskWithNoSchedule() {
  QJsonObject kanban_task;
  kanban_task.insert("id", "task-from-kanban");
  kanban_task.insert("text", "From Kanban");
  kanban_task.insert("column", "todo");

  QJsonObject board;
  board.insert("tasks", QJsonArray{kanban_task});
  board.insert("columns", QJsonArray{QJsonObject{{"id", "todo"}, {"label", "To do"}}});
  board.insert("links", QJsonArray{});

  ProjectBoardGanttModel model;
  model.LoadFromJson(board);

  const auto index = model.IndexForTaskId("task-from-kanban");
  Require(index.isValid(), "task-from-kanban should be found by id");

  const auto start = model.data(index, KDGantt::StartTimeRole).toDateTime();
  const auto end = model.data(index, KDGantt::EndTimeRole).toDateTime();
  Require(start.isValid(), "a task with no start/end/duration should still get a valid start date");
  Require(end.isValid(), "a task with no start/end/duration should still get a valid end date");
  Require(end > start, "the defaulted end date should be after the defaulted start date");
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

  // The widget's constraintAdded handler defers its re-theming to the next event-loop iteration
  // (see project_board_gantt_widget.cc for why) rather than acting reentrant inside this same
  // signal dispatch, so give it a chance to run before checking the pen.
  QCoreApplication::processEvents();

  // A link added this way (bare Constraint, no pen data) is exactly what KDGantt's own internal
  // mouse handling does when the user drags a new dependency link interactively -- without the
  // widget's constraintAdded handler re-theming it, this would fall back to KDGantt::ItemDelegate's
  // hardcoded black/red pen instead of matching the white pen LoadFromJson() applies on load.
  bool found_new_link = false;
  for (const auto& constraint : model->ConstraintModel()->constraints()) {
    if (constraint.startIndex() != design_index || constraint.endIndex() != milestone_index) {
      continue;
    }
    found_new_link = true;
    Require(constraint.data(KDGantt::Constraint::ValidConstraintPen).value<QPen>() ==
                ProjectBoardGanttModel::LinkPen(),
            "an interactively-created link should get the themed pen, not KDGantt's default");
  }
  Require(found_new_link, "the newly-added constraint should be found in the constraint model");
}

// Unlike TestAddingADependencyLinkEmitsDataChanged (which adds directly to
// Model()->ConstraintModel()), this drives KDGantt::GraphicsView::addConstraint() -- the exact
// method GraphicsItem::mouseReleaseEvent() calls for a real interactive drag-to-link gesture.
// KDGantt::View keeps a *separate*, internally address-mapped ConstraintModel for rendering/
// interaction (see kdganttview.cpp's mappedConstraintModel/ConstraintProxy) that
// Model()->ConstraintModel() does not expose -- this checks whether that separate model ends up
// with duplicate/mis-themed ConstraintGraphicsItems for the same link.
void TestInteractivelyCreatedLinkGetsThemedWithoutDuplicates() {
  ProjectBoardGanttWidget widget;
  widget.LoadFromJson(SampleBoard());

  auto* model = widget.Model();
  const auto design_index = model->IndexForTaskId("task-design");
  const auto milestone_index = model->IndexForTaskId("milestone-review");
  Require(design_index.isValid() && milestone_index.isValid(),
          "both endpoints of the new link should be found by id");

  // GraphicsItem::mouseReleaseEvent() maps only from the scene's own model (summaryHandlingModel)
  // down to View::ganttProxyModel()'s index space before calling GraphicsView::addConstraint() --
  // NOT all the way back to Model()'s indices. Passing raw Model() indices instead crashes
  // ForwardingProxyModel::mapToSource() downstream (Q_ASSERT(proxyIndex.model() == this)), so
  // mimic that one level of mapping here too.
  auto* gantt_proxy = widget.View()->ganttProxyModel();
  widget.View()->graphicsView()->addConstraint(gantt_proxy->mapFromSource(design_index),
                                               gantt_proxy->mapFromSource(milestone_index),
                                               Qt::NoModifier);
  // The widget's constraintAdded handler defers its re-theming to the next event-loop iteration
  // (see project_board_gantt_widget.cc for why) -- give it a chance to run.
  QCoreApplication::processEvents();

  auto* rendered_model = widget.View()->graphicsView()->constraintModel();
  int matching_count = 0;
  int themed_count = 0;
  for (const auto& constraint : rendered_model->constraints()) {
    // The rendered/mapped model's indices are proxy-mapped, not equal to design_index/
    // milestone_index directly -- match structurally instead (top-level rows 0 and 1, the only
    // pair that can be design->milestone in this 3-task board).
    if (constraint.startIndex().row() != 0 || constraint.startIndex().parent().isValid() ||
        constraint.endIndex().row() != 1 || constraint.endIndex().parent().isValid()) {
      continue;
    }
    ++matching_count;
    if (constraint.data(KDGantt::Constraint::ValidConstraintPen).isValid()) {
      ++themed_count;
    }
  }
  Require(matching_count == 1,
          "exactly one rendered constraint should exist for the new link -- more than one means a "
          "stale/duplicate item is overlapping the themed one");
  Require(themed_count == matching_count,
          "the rendered link should carry the themed pen, not KDGantt's black/red default");
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

void TestTwoSequentialInteractivelyCreatedLinksBothGetThemed() {
  QJsonObject board;
  board.insert("tasks",
               QJsonArray{MakeTask("a", 1), MakeTask("b", 1), MakeTask("c", 1), MakeTask("d", 1)});
  board.insert("links", QJsonArray{});

  ProjectBoardGanttWidget widget;
  widget.LoadFromJson(board);

  auto* model = widget.Model();
  const auto a_index = model->IndexForTaskId("a");
  const auto b_index = model->IndexForTaskId("b");
  const auto c_index = model->IndexForTaskId("c");
  const auto d_index = model->IndexForTaskId("d");
  Require(a_index.isValid() && b_index.isValid() && c_index.isValid() && d_index.isValid(),
          "all four tasks should be found by id");

  auto* gantt_proxy = widget.View()->ganttProxyModel();
  auto* graphics_view = widget.View()->graphicsView();
  graphics_view->addConstraint(gantt_proxy->mapFromSource(a_index),
                               gantt_proxy->mapFromSource(b_index), Qt::NoModifier);
  graphics_view->addConstraint(gantt_proxy->mapFromSource(c_index),
                               gantt_proxy->mapFromSource(d_index), Qt::NoModifier);
  // The widget's constraintAdded handler defers its re-theming to the next event-loop iteration
  // (see project_board_gantt_widget.cc for why) -- give it a chance to run for both links.
  QCoreApplication::processEvents();

  auto IsThemed = [&](int start_row, int end_row) {
    for (const auto& constraint : graphics_view->constraintModel()->constraints()) {
      if (constraint.startIndex().row() != start_row ||
          constraint.startIndex().parent().isValid() || constraint.endIndex().row() != end_row ||
          constraint.endIndex().parent().isValid()) {
        continue;
      }
      return constraint.data(KDGantt::Constraint::ValidConstraintPen).isValid();
    }
    return false;
  };
  Require(IsThemed(0, 1), "the first interactively-created link (a->b) should be themed");
  Require(IsThemed(2, 3), "the second interactively-created link (c->d) should be themed");
}

// Unlike the two prior interactive-creation tests (both endpoints top-level, or one endpoint a
// summary with no nested-child involvement), this link's SOURCE endpoint is a *child* row nested
// under a summary (task-wireframes, nested under task-design) -- SampleBoard() already has one
// pre-existing link with exactly this shape (task-wireframes -> milestone-review) loaded via
// LoadFromJson(), so this adds a SECOND, freshly interactive one between the same kind of nested
// child and a different top-level target, to see whether nesting specifically breaks theming.
void TestInteractivelyCreatedLinkFromNestedChildGetsThemed() {
  ProjectBoardGanttWidget widget;
  widget.LoadFromJson(SampleBoard());

  auto* model = widget.Model();
  const auto wireframes_index = model->IndexForTaskId("task-wireframes");
  const auto milestone_index = model->IndexForTaskId("milestone-review");
  Require(wireframes_index.isValid() && milestone_index.isValid(),
          "task-wireframes (nested child) and milestone-review should be found by id");
  Require(wireframes_index.parent().isValid(), "task-wireframes should indeed be a nested child");

  auto* gantt_proxy = widget.View()->ganttProxyModel();
  auto* graphics_view = widget.View()->graphicsView();
  // SampleBoard() already links task-wireframes -> milestone-review, so hasConstraint() would
  // just toggle that one off -- link task-design (the nested child's *parent*, not itself nested)
  // to milestone-review instead, still exercising a not-purely-top-level source endpoint without
  // colliding with the pre-existing link.
  const auto design_index = model->IndexForTaskId("task-design");
  graphics_view->addConstraint(gantt_proxy->mapFromSource(wireframes_index),
                               gantt_proxy->mapFromSource(design_index), Qt::NoModifier);
  // The widget's constraintAdded handler defers its re-theming to the next event-loop iteration
  // (see project_board_gantt_widget.cc for why) -- give it a chance to run.
  QCoreApplication::processEvents();

  // task-design is top-level row 0; milestone-review is top-level row 1 -- match end row 0 to
  // isolate this test's new wireframes->design link from SampleBoard()'s pre-existing
  // wireframes->milestone-review one (end row 1), which was already themed by LoadFromJson().
  bool found = false;
  bool themed = false;
  for (const auto& constraint : graphics_view->constraintModel()->constraints()) {
    if (!constraint.startIndex().parent().isValid() || constraint.endIndex().parent().isValid() ||
        constraint.endIndex().row() != 0) {
      continue;
    }
    found = true;
    themed = themed || constraint.data(KDGantt::Constraint::ValidConstraintPen).isValid();
  }
  Require(found, "the new nested-child-sourced constraint (wireframes->design) should exist");
  Require(themed, "the interactively-created link from a nested child should be themed");
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
  TestLoadFromJsonDefaultsStartDateForTaskWithNoSchedule();
  TestLoadFromJsonBuildsSummaryHierarchyForKdGantt();
  TestLoadFromJsonDoesNotEmitDataChanged();
  TestDraggingATaskBarEmitsDataChangedWithUpdatedTask();
  TestAddingADependencyLinkEmitsDataChanged();
  TestInteractivelyCreatedLinkGetsThemedWithoutDuplicates();
  TestTwoSequentialInteractivelyCreatedLinksBothGetThemed();
  TestInteractivelyCreatedLinkFromNestedChildGetsThemed();
  TestCriticalPathFollowsLongestChain();
  TestCriticalPathEmptyOnCycle();
  TestCriticalPathWithNoLinksPicksLongestTask();

  return EXIT_SUCCESS;
}
