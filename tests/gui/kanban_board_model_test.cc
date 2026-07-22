#include "gui/project_board/kanban/kanban_board_model.h"

#include <QCoreApplication>
#include <QObject>
#include <cstdlib>
#include <functional>
#include <iostream>

#include "gui/project_board/kanban/kanban_board_document.h"

namespace {

using cppwiki::gui::kanban::KanbanBoardDocument;
using cppwiki::gui::kanban::KanbanBoardModel;
using cppwiki::gui::kanban::kPriorityHigh;

void Require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

constexpr auto kSampleJson = R"json({
  "tasks": [
    {"id": "t1", "text": "Alpha", "column": "todo", "priority": 1, "progress": 0}
  ],
  "columns": [
    {"id": "todo", "label": "To do"},
    {"id": "done", "label": "Done"}
  ]
})json";

void LoadSampleDocument(KanbanBoardModel* model) {
  const auto document = KanbanBoardDocument::ParseJson(QByteArray(kSampleJson));
  Require(document.has_value(), "sample JSON should parse successfully");
  model->SetDocument(*document);
}

int CountBoardChanged(KanbanBoardModel& model, const std::function<void()>& action) {
  int count = 0;
  QObject::connect(&model, &KanbanBoardModel::boardChanged, [&count]() { ++count; });
  action();
  return count;
}

void TestAddColumnAppendsWithFreshId() {
  KanbanBoardModel model;
  LoadSampleDocument(&model);

  const int emit_count =
      CountBoardChanged(model, [&model]() { model.addColumn(QStringLiteral("In review")); });

  const auto document = model.ExportDocument();
  Require(document.columns.size() == 3, "addColumn should append a new column");
  Require(document.columns.last().label == QStringLiteral("In review"),
          "new column should have the requested label");
  Require(!document.columns.last().id.isEmpty(), "new column should get a non-empty id");
  Require(document.columns.last().id != document.columns.first().id,
          "new column's id should not collide with an existing one");
  Require(emit_count == 1, "addColumn should emit boardChanged");
}

void TestAddTaskAppendsUnparentedTask() {
  KanbanBoardModel model;
  LoadSampleDocument(&model);

  const int emit_count = CountBoardChanged(model, [&model]() {
    model.addTask(QStringLiteral("Bravo"), QStringLiteral("done"), kPriorityHigh, 25);
  });

  const auto document = model.ExportDocument();
  Require(document.tasks.size() == 2, "addTask should append a new task");
  const auto& added = document.tasks.last();
  Require(added.text == QStringLiteral("Bravo"), "new task should have the requested text");
  Require(added.column == QStringLiteral("done"), "new task should land in the requested column");
  Require(added.priority == kPriorityHigh, "new task should have the requested priority");
  Require(added.progress == 25, "new task should have the requested progress");
  Require(added.parent.isEmpty(), "a task created via the toolbar has no epic/parent");
  Require(!added.id.isEmpty(), "new task should get a non-empty id");
  Require(added.id != document.tasks.first().id, "new task's id should not collide");
  Require(emit_count == 1, "addTask should emit boardChanged");
}

void TestUpdateTaskEditsInPlace() {
  KanbanBoardModel model;
  LoadSampleDocument(&model);

  const int emit_count = CountBoardChanged(model, [&model]() {
    model.updateTask(QStringLiteral("t1"), QStringLiteral("Alpha (renamed)"),
                     QStringLiteral("done"), kPriorityHigh, 80);
  });

  const auto document = model.ExportDocument();
  Require(document.tasks.size() == 1, "updateTask should not add or remove tasks");
  Require(document.tasks.first().text == QStringLiteral("Alpha (renamed)"), "text edit stuck");
  Require(document.tasks.first().column == QStringLiteral("done"), "column edit stuck");
  Require(document.tasks.first().priority == kPriorityHigh, "priority edit stuck");
  Require(document.tasks.first().progress == 80, "progress edit stuck");
  Require(emit_count == 1, "updateTask should emit boardChanged");
}

void TestUpdateTaskIsNoOpForUnknownId() {
  KanbanBoardModel model;
  LoadSampleDocument(&model);

  const int emit_count = CountBoardChanged(model, [&model]() {
    model.updateTask(QStringLiteral("does-not-exist"), QStringLiteral("x"), QStringLiteral("todo"),
                     0, 0);
  });

  Require(model.ExportDocument().tasks.first().text == QStringLiteral("Alpha"),
          "updateTask with an unknown id should leave the existing task untouched");
  Require(emit_count == 0, "updateTask should not emit boardChanged for an unknown id");
}

void TestFindTaskAndFirstColumnId() {
  KanbanBoardModel model;
  LoadSampleDocument(&model);

  const auto found = model.FindTask(QStringLiteral("t1"));
  Require(found.has_value(), "FindTask should find an existing task");
  Require(found->text == QStringLiteral("Alpha"), "FindTask should return the matching task");

  Require(!model.FindTask(QStringLiteral("missing")).has_value(),
          "FindTask should return nullopt for an unknown id");

  Require(model.FirstColumnId() == QStringLiteral("todo"),
          "FirstColumnId should return the board's first column");
}

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication application(argc, argv);

  TestAddColumnAppendsWithFreshId();
  TestAddTaskAppendsUnparentedTask();
  TestUpdateTaskEditsInPlace();
  TestUpdateTaskIsNoOpForUnknownId();
  TestFindTaskAndFirstColumnId();

  std::cout << "All kanban_board_model tests passed\n";
  return EXIT_SUCCESS;
}
