#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_TASK_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_TASK_H_

#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <optional>

namespace cppwiki::gui::project_board {

// Kanban's built-in priority levels (see frontend/editor/src/project/projectBoard.ts's `priority`
// field and ProjectBoardView.tsx's getPriorityOptions()) — 1 = Low, 2 = Medium, 3 = High. There is
// no "0" level; a task with no `priority` key at all is simply unprioritized (see
// ProjectTask::hasPriority()).
constexpr int kPriorityLow = 1;
constexpr int kPriorityMedium = 2;
constexpr int kPriorityHigh = 3;

// Human label for one of the three priority levels above; empty for anything else.
[[nodiscard]] QString PriorityLabel(int priority);

// A single row of the shared Project board task list (see projectBoard.ts's `ProjectTask`). This
// wraps the task's JSON object directly rather than copying every field into typed members: the
// Table view only reads/edits a handful of fields (text/start/duration/progress/column/priority),
// but the full schema also carries `end`/`parent`/`type`/`tags`/`users`/`description`/`deadline`,
// none of which this view renders. Storing the live QJsonObject as the single source of truth
// means those untouched fields round-trip through toJson() automatically — there's no separate
// "extra fields" bucket that could fall out of sync with what's actually being edited.
class ProjectTask {
 public:
  ProjectTask() = default;
  explicit ProjectTask(QJsonObject json) : json_(std::move(json)) {}

  [[nodiscard]] QString id() const;
  void setId(const QString& id);

  [[nodiscard]] QString text() const;
  void setText(const QString& text);

  // Stored in JSON as an ISO 8601 date-time string (see projectBoard.ts's `start`); an invalid
  // QDate is returned if the field is missing or unparseable.
  [[nodiscard]] QDate start() const;
  void setStart(const QDate& date);

  [[nodiscard]] int duration() const;
  void setDuration(int duration);

  [[nodiscard]] int progress() const;
  void setProgress(int progress);

  // The board's status column id this task currently belongs to (see ProjectColumn::id).
  [[nodiscard]] QString column() const;
  void setColumn(const QString& column_id);

  // `priority` is optional in the schema (a task may have no priority at all) — hasPriority()
  // distinguishes "no priority set" from "priority explicitly 0" (which never occurs, but is not
  // assumed away here).
  [[nodiscard]] bool hasPriority() const;
  [[nodiscard]] int priority() const;
  void setPriority(int priority);
  void clearPriority();

  [[nodiscard]] const QJsonObject& toJson() const {
    return json_;
  }

 private:
  QJsonObject json_;
};

// A board status column (see projectBoard.ts's `ProjectColumn`) — just an id/label pair, no
// optional fields to preserve.
struct ProjectColumn {
  QString id;
  QString label;
};

[[nodiscard]] QJsonObject ToJson(const ProjectColumn& column);
[[nodiscard]] ProjectColumn ColumnFromJson(const QJsonObject& json);

// The full Project board document (see projectBoard.ts's `ProjectBoard`). `links` (Gantt-only task
// dependency links) is preserved opaquely — this view neither reads nor edits it, just carries it
// through so round-tripping a document through this component never drops Gantt's data.
struct ProjectBoardDocument {
  QVector<ProjectTask> tasks;
  QVector<ProjectColumn> columns;
  QJsonArray links;
};

// Mirrors parseProjectBoardJson() in projectBoard.ts: empty content parses to an empty task list
// with the default To do/In progress/Done columns; malformed JSON (or JSON missing the required
// `tasks`/`columns` arrays) returns std::nullopt.
[[nodiscard]] std::optional<ProjectBoardDocument> ParseProjectBoardJson(const QString& raw_content);

// Serializes back to the same JSON shape parseProjectBoardJson()/ParseProjectBoardJson() read.
[[nodiscard]] QString SerializeProjectBoardJson(const ProjectBoardDocument& document);

}  // namespace cppwiki::gui::project_board

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_TASK_H_
