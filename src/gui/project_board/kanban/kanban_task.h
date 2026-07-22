#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_TASK_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_TASK_H_

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace cppwiki::gui::kanban {

// Kanban's priority levels (see frontend/editor/src/project/projectBoard.ts's `priority` field) —
// 1 = Low, 2 = Medium, 3 = High; 0/unset means no priority.
constexpr int kPriorityLow = 1;
constexpr int kPriorityMedium = 2;
constexpr int kPriorityHigh = 3;

// Human label for one of the three priority levels above; empty for anything else (including 0).
[[nodiscard]] auto PriorityLabel(int priority) -> QString;

// Mirrors the `ProjectTask` shape defined in frontend/editor/src/project/projectBoard.ts — the
// schema shared by all three Project board views (Gantt/Kanban/Table). This native Kanban MVP
// only renders/edits a subset of these fields (text, column, parent/epic, priority, progress);
// the rest (tags, users, dates beyond `start`, description) round-trip through JSON untouched so
// documents saved from here don't lose data the web view still relies on. See the parent issue
// for what's deferred.
struct KanbanTask {
  QString id;
  QString text;
  QString start;
  double duration = 0;
  QString end;
  double progress = 0;
  QString column;
  QString parent;
  // "task" | "summary" | "milestone"; empty is treated as "task". Epics are tasks with
  // type == "summary" (see IsEpic()).
  QString type;
  QStringList tags;
  QStringList users;
  // Kanban's priority levels: 1 = Low, 2 = Medium, 3 = High, 0/unset = none.
  int priority = 0;
  QString description;
  QString deadline;

  [[nodiscard]] auto IsEpic() const -> bool {
    return type == QLatin1String("summary");
  }

  [[nodiscard]] static auto FromJson(const QJsonObject& obj) -> KanbanTask;
  [[nodiscard]] auto ToJson() const -> QJsonObject;
};

}  // namespace cppwiki::gui::kanban

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_KANBAN_KANBAN_TASK_H_
