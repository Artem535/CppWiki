#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_PROJECT_BOARD_NATIVE_WIDGET_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_PROJECT_BOARD_NATIVE_WIDGET_H_

#include <QByteArray>
#include <QJsonArray>
#include <QWidget>

class QTabWidget;

namespace cppwiki::gui::project_board::gantt {
class ProjectBoardGanttWidget;
}  // namespace cppwiki::gui::project_board::gantt

namespace cppwiki::gui::kanban {
class KanbanBoardWidget;
}  // namespace cppwiki::gui::kanban

namespace cppwiki::gui::project_board {

class ProjectTaskTableWidget;

// Native (non-web) renderer for the Project board document kind (issue #113/#127) — combines the
// three standalone widgets built for #113 (ProjectBoardGanttWidget, kanban::KanbanBoardWidget,
// ProjectTaskTableWidget) into one QTabWidget, mirroring the tab structure of the web renderer
// this replaces (frontend/editor/src/project/ProjectBoardView.tsx). All three tabs share the same
// underlying `{tasks, columns, links}` JSON document (see projectBoard.ts); whichever tab the
// user edits becomes the source of truth for that edit and is immediately re-broadcast to the
// other two so they stay in sync, then documentEdited() fires so the host (Page) can persist it —
// mirroring ProjectBoardView.tsx's single `board` state + commitBoard() funnel.
//
// Only ProjectBoardGanttWidget's JSON includes `links` (kanban::KanbanBoardWidget only knows
// `{tasks, columns}`; ProjectTaskTableWidget carries `links` through opaquely without reading or
// editing it) — see LoadFromJson()/the *Edited() handlers for how `links` is preserved across an
// edit made from any of the three tabs, including ones that don't know it exists.
class ProjectBoardNativeWidget final : public QWidget {
  Q_OBJECT

 public:
  explicit ProjectBoardNativeWidget(QWidget* parent = nullptr);

  // Replaces the currently displayed board with `json` (a `{tasks, columns, links}` document —
  // the same shape DocumentValidator accepts for DocumentKind::kProjectBoard). Safe to call
  // repeatedly (e.g. when Page switches to a different project-board document).
  void LoadFromJson(const QByteArray& json);

  // The current board state, reflecting any edits made since LoadFromJson().
  [[nodiscard]] QByteArray ToJson() const;

 signals:
  // Emitted whenever the user edits a task, a dependency link, or a Kanban column/swimlane
  // placement in any of the three tabs. Callers persist via ToJson().
  void documentEdited();

 private:
  void HandleGanttEdited();
  void HandleKanbanEdited();
  void HandleTableEdited();

  QTabWidget* tabs_ = nullptr;
  gantt::ProjectBoardGanttWidget* gantt_widget_ = nullptr;
  kanban::KanbanBoardWidget* kanban_widget_ = nullptr;
  ProjectTaskTableWidget* table_widget_ = nullptr;

  // Gantt is the only tab that knows about dependency links; carried here so an edit made from
  // Kanban or Table (neither of which round-trips `links` into their own edit output) doesn't
  // silently drop it when re-broadcast to the other tabs. Updated from Gantt's own ToJson()
  // whenever Gantt edits, and initialized from the loaded document in LoadFromJson().
  QJsonArray links_;

  // Guards LoadFromJson() itself from being mistaken for a user edit: each inner widget emits its
  // own "changed" signal in response to being loaded (KanbanBoardModel::SetDocument() notifies its
  // Q_PROPERTYs unconditionally; Gantt/Table each already guard their *own* signal internally, but
  // this is the belt-and-suspenders guard at this widget's level).
  bool loading_ = false;
};

}  // namespace cppwiki::gui::project_board

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_PROJECT_BOARD_NATIVE_WIDGET_H_
