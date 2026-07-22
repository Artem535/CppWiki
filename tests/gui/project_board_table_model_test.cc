#include <QCoreApplication>
#include <QDate>
#include <QString>
#include <cstdlib>
#include <iostream>

#include "gui/project_board/table/project_task.h"
#include "gui/project_board/table/project_task_table_model.h"

namespace {

using cppwiki::gui::project_board::ParseProjectBoardJson;
using cppwiki::gui::project_board::ProjectBoardDocument;
using cppwiki::gui::project_board::ProjectColumn;
using cppwiki::gui::project_board::ProjectTask;
using cppwiki::gui::project_board::ProjectTaskTableModel;
using cppwiki::gui::project_board::SerializeProjectBoardJson;

void Require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

constexpr auto kSampleJson = R"json({
  "tasks": [
    {"id": "t1", "text": "Alpha", "start": "2026-07-24T00:00:00.000Z", "duration": 3,
     "progress": 50, "column": "todo", "priority": 2, "tags": ["x"]},
    {"id": "t2", "text": "Bravo", "start": "2026-07-20T00:00:00.000Z", "duration": 1,
     "progress": 100, "column": "done", "priority": 3},
    {"id": "t3", "text": "Charlie", "start": "2026-07-22T00:00:00.000Z", "duration": 5,
     "progress": 0, "column": "inProgress"}
  ],
  "columns": [
    {"id": "todo", "label": "To do"},
    {"id": "inProgress", "label": "In progress"},
    {"id": "done", "label": "Done"}
  ],
  "links": [{"id": "l1", "type": "e2s", "source": "t2", "target": "t1"}]
})json";

ProjectBoardDocument LoadSampleDocument() {
  const auto document = ParseProjectBoardJson(QString::fromUtf8(kSampleJson));
  Require(document.has_value(), "sample JSON should parse successfully");
  return *document;
}

void TestEmptyContentParsesToDefaultColumns() {
  const auto document = ParseProjectBoardJson(QString());
  Require(document.has_value(), "empty content should parse, not fail");
  Require(document->tasks.isEmpty(), "empty content should have no tasks");
  Require(document->columns.size() == 3, "empty content should get the three default columns");
  Require(document->columns.at(0).id == QStringLiteral("todo"), "first default column is todo");
}

void TestMalformedJsonFailsToParse() {
  Require(!ParseProjectBoardJson(QStringLiteral("{not valid json")).has_value(),
          "malformed JSON should fail to parse");
  Require(!ParseProjectBoardJson(QStringLiteral("{\"tasks\": []}")).has_value(),
          "JSON missing the required columns array should fail to parse");
}

void TestModelExposesTaskFieldsAcrossColumns() {
  const auto document = LoadSampleDocument();
  ProjectTaskTableModel model;
  model.setBoardColumns(document.columns);
  model.setTasks(document.tasks);

  Require(model.rowCount() == 3, "model should expose all three sample tasks");
  Require(model.columnCount() == ProjectTaskTableModel::kColumnCount, "model has six columns");

  const QModelIndex task_index = model.index(0, ProjectTaskTableModel::kTaskColumn);
  Require(model.data(task_index, Qt::DisplayRole).toString() == QStringLiteral("Alpha"),
          "Task column should show the task's text");

  const QModelIndex status_index = model.index(0, ProjectTaskTableModel::kStatusColumn);
  Require(model.data(status_index, Qt::DisplayRole).toString() == QStringLiteral("To do"),
          "Status column should show the matching board column's label");
  Require(model.data(status_index, Qt::EditRole).toString() == QStringLiteral("todo"),
          "Status column's edit role should expose the raw column id");
  Require(model.data(status_index, ProjectTaskTableModel::kToneRole).toInt() == 0,
          "first board column should have tone 0");

  const QModelIndex priority_index = model.index(0, ProjectTaskTableModel::kPriorityColumn);
  Require(model.data(priority_index, Qt::DisplayRole).toString() == QStringLiteral("Medium"),
          "Priority column should show the human label for priority 2");

  const QModelIndex no_priority_index = model.index(2, ProjectTaskTableModel::kPriorityColumn);
  Require(model.data(no_priority_index, Qt::DisplayRole).toString().isEmpty(),
          "a task with no priority set should show an empty Priority cell");
  Require(model.data(no_priority_index, ProjectTaskTableModel::kToneRole).toInt() == -1,
          "a task with no priority set should report tone -1");

  const QModelIndex start_index = model.index(0, ProjectTaskTableModel::kStartColumn);
  Require(model.data(start_index, Qt::DisplayRole).toString() == QStringLiteral("Jul 24, 2026"),
          "Start column should render a readable 'MMM d, yyyy' date, not QDate::toString()'s "
          "default");
  Require(model.data(start_index, Qt::EditRole).toDate() == QDate(2026, 7, 24),
          "Start column's edit role should expose a real QDate");

  const QModelIndex duration_index = model.index(0, ProjectTaskTableModel::kDurationColumn);
  Require(model.data(duration_index, Qt::DisplayRole).toInt() == 3, "Duration column shows days");

  const QModelIndex progress_index = model.index(0, ProjectTaskTableModel::kProgressColumn);
  Require(model.data(progress_index, Qt::DisplayRole).toString() == QStringLiteral("50%"),
          "Progress column shows a percentage");
}

void TestUnknownStatusColumnFallsBackToUnassigned() {
  ProjectTaskTableModel model;
  model.setBoardColumns({ProjectColumn{QStringLiteral("todo"), QStringLiteral("To do")}});
  ProjectTask orphaned;
  orphaned.setId(QStringLiteral("t-orphan"));
  orphaned.setText(QStringLiteral("Orphan"));
  orphaned.setColumn(QStringLiteral("does-not-exist"));
  model.setTasks({orphaned});

  const QModelIndex status_index = model.index(0, ProjectTaskTableModel::kStatusColumn);
  Require(model.data(status_index, Qt::DisplayRole).toString() == QStringLiteral("Unassigned"),
          "a task pointing at a deleted column should show 'Unassigned', not a raw id");
  Require(model.data(status_index, ProjectTaskTableModel::kToneRole).toInt() == -1,
          "an unassigned status should report tone -1");
}

void TestSetDataEditsRoundTripThroughJson() {
  auto document = LoadSampleDocument();
  ProjectTaskTableModel model;
  model.setBoardColumns(document.columns);
  model.setTasks(document.tasks);

  int data_changed_count = 0;
  QObject::connect(&model, &ProjectTaskTableModel::dataChanged,
                   [&data_changed_count]() { ++data_changed_count; });

  Require(model.setData(model.index(0, ProjectTaskTableModel::kTaskColumn),
                        QStringLiteral("Alpha (renamed)")),
          "setData on Task column should succeed");
  Require(
      model.setData(model.index(0, ProjectTaskTableModel::kStatusColumn), QStringLiteral("done")),
      "setData on Status column should succeed");
  Require(model.setData(model.index(2, ProjectTaskTableModel::kPriorityColumn), 1),
          "setData on Priority column should succeed");
  Require(model.setData(model.index(0, ProjectTaskTableModel::kStartColumn), QDate(2026, 8, 1)),
          "setData on Start column should succeed");
  Require(model.setData(model.index(0, ProjectTaskTableModel::kDurationColumn), 9),
          "setData on Duration column should succeed");
  Require(model.setData(model.index(0, ProjectTaskTableModel::kProgressColumn), 200),
          "setData on Progress column should succeed even with an out-of-range value");

  Require(data_changed_count > 0, "setData should emit dataChanged");

  Require(model.taskAt(0).text() == QStringLiteral("Alpha (renamed)"), "text edit stuck");
  Require(model.taskAt(0).column() == QStringLiteral("done"), "status edit stuck");
  Require(model.taskAt(2).priority() == 1, "priority edit stuck");
  Require(model.taskAt(0).start() == QDate(2026, 8, 1), "start date edit stuck");
  Require(model.taskAt(0).duration() == 9, "duration edit stuck");
  Require(model.taskAt(0).progress() == 100, "progress should be clamped to 100");

  // The full document (tags/links included) should round-trip losslessly even though the Table
  // view never renders tags or links: re-parsing the serialized output should still carry task
  // t1's "tags": ["x"] and the one Gantt dependency link.
  ProjectBoardDocument roundtrip;
  roundtrip.tasks = model.tasks();
  roundtrip.columns = model.boardColumns();
  roundtrip.links = document.links;
  const QString serialized = SerializeProjectBoardJson(roundtrip);
  const auto reparsed = ParseProjectBoardJson(serialized);
  Require(reparsed.has_value(), "serialized document should re-parse");
  Require(reparsed->tasks.at(0).toJson().value(QStringLiteral("tags")).toArray().size() == 1,
          "tags not shown in this view should still round-trip");
  Require(reparsed->links.size() == 1, "Gantt-only links should still round-trip");
}

void TestClearingPriority() {
  auto document = LoadSampleDocument();
  ProjectTaskTableModel model;
  model.setBoardColumns(document.columns);
  model.setTasks(document.tasks);

  Require(model.taskAt(0).hasPriority(), "task 0 starts with a priority");
  Require(model.setData(model.index(0, ProjectTaskTableModel::kPriorityColumn), 0),
          "setData with priority 0 should succeed (clears priority)");
  Require(!model.taskAt(0).hasPriority(), "priority 0 should clear the priority field");
}

void TestSortByEachColumn() {
  auto document = LoadSampleDocument();
  ProjectTaskTableModel model;
  model.setBoardColumns(document.columns);
  model.setTasks(document.tasks);

  model.sort(ProjectTaskTableModel::kTaskColumn, Qt::AscendingOrder);
  Require(model.taskAt(0).text() == QStringLiteral("Alpha"), "sort by Task ascending: Alpha first");
  Require(model.taskAt(2).text() == QStringLiteral("Charlie"),
          "sort by Task ascending: Charlie last");

  model.sort(ProjectTaskTableModel::kTaskColumn, Qt::DescendingOrder);
  Require(model.taskAt(0).text() == QStringLiteral("Charlie"),
          "sort by Task descending: Charlie first");

  // Status sorts by workflow position (todo -> inProgress -> done), not alphabetically.
  model.sort(ProjectTaskTableModel::kStatusColumn, Qt::AscendingOrder);
  Require(model.taskAt(0).column() == QStringLiteral("todo"), "todo sorts first by workflow order");
  Require(model.taskAt(2).column() == QStringLiteral("done"), "done sorts last by workflow order");

  model.sort(ProjectTaskTableModel::kPriorityColumn, Qt::AscendingOrder);
  Require(!model.taskAt(0).hasPriority(), "unprioritized task sorts before any priority level");
  Require(model.taskAt(2).priority() == 3, "High priority sorts last ascending");

  model.sort(ProjectTaskTableModel::kStartColumn, Qt::AscendingOrder);
  Require(model.taskAt(0).text() == QStringLiteral("Bravo"), "earliest start date sorts first");

  model.sort(ProjectTaskTableModel::kDurationColumn, Qt::AscendingOrder);
  Require(model.taskAt(0).duration() == 1, "shortest duration sorts first");

  model.sort(ProjectTaskTableModel::kProgressColumn, Qt::DescendingOrder);
  Require(model.taskAt(0).progress() == 100, "highest progress sorts first descending");
}

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication application(argc, argv);

  TestEmptyContentParsesToDefaultColumns();
  TestMalformedJsonFailsToParse();
  TestModelExposesTaskFieldsAcrossColumns();
  TestUnknownStatusColumnFallsBackToUnassigned();
  TestSetDataEditsRoundTripThroughJson();
  TestClearingPriority();
  TestSortByEachColumn();

  std::cout << "All project_board_table_model tests passed\n";
  return EXIT_SUCCESS;
}
