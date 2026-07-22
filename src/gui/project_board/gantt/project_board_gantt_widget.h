#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_GANTT_PROJECT_BOARD_GANTT_WIDGET_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_GANTT_PROJECT_BOARD_GANTT_WIDGET_H_

#include <QJsonObject>
#include <QWidget>
#include <memory>

class QEvent;

namespace KDGantt {
class View;
}  // namespace KDGantt

namespace cppwiki::gui::project_board::gantt {

class ProjectBoardGanttModel;

// Standalone native-Qt replacement for the Project board document kind's web-based (SVAR
// react-gantt, see frontend/editor/src/project/ProjectBoardView.tsx) Gantt tab — see issue #113.
// Wraps KDGantt::View (KDAB's KDChart, MIT-licensed) instead of a QWebEngineView-hosted React
// component; being a native model/view widget, it doesn't reinitialize its internal state on
// every task/link edit the way the SVAR Gantt does (see #112), and its undo/redo would run
// through this app's own history rather than a gated-behind-a-paid-tier library feature.
//
// This widget is NOT wired into IPage/Page/MainWindow yet — that integration (routing
// DocumentKind::kProjectBoard's Gantt tab to this widget instead of the web view) is tracked as
// follow-up work. It reads/writes the exact same `{ tasks, columns, links }` JSON schema as the
// web renderer (see ProjectBoardGanttModel), so a document saved by one is loadable by the other.
class ProjectBoardGanttWidget final : public QWidget {
  Q_OBJECT

 public:
  explicit ProjectBoardGanttWidget(QWidget* parent = nullptr);
  ~ProjectBoardGanttWidget() override;

  // Replaces the currently displayed board with `board` (a `{ tasks, columns, links }` JSON
  // object matching ProjectBoard in projectBoard.ts). Safe to call repeatedly (e.g. when the
  // host page switches documents).
  void LoadFromJson(const QJsonObject& board);

  // Returns the current board state as JSON, reflecting any edits made since LoadFromJson().
  [[nodiscard]] auto ToJson() const -> QJsonObject;

  // Non-owning access to the underlying model, exposed the same way QTableView::model() etc.
  // are: useful for tests, and for a future integration layer that needs direct model access
  // (e.g. syncing selection with the Kanban/Table views over the same document).
  [[nodiscard]] auto Model() const -> ProjectBoardGanttModel*;

 signals:
  // Emitted whenever the user edits a task (drags/resizes a bar, edits inline via the left tree)
  // or a dependency link (drawn/deleted in the Gantt view). Carries the full board as JSON via
  // ToJson() — persistence/save wiring is left to the integration step, not this widget.
  void DataChanged(QJsonObject board);

 private:
  void EmitDataChanged();
  // Redirects plain (unmodified) mouse-wheel scrolling on the chart area to the horizontal
  // scrollbar instead of the vertical one: KDGantt::View always shows a horizontal scrollbar
  // (kdganttview.cpp sets Qt::ScrollBarAlwaysOn) since the timeline is usually wider than the
  // viewport, but a plain wheel only drives Qt's default *vertical* scrollbar, so users had no
  // obvious way to pan the timeline without grabbing the thin scrollbar handle directly.
  bool eventFilter(QObject* watched, QEvent* event) override;

  void HandleCriticalPathToggled(bool checked);

  KDGantt::View* view_ = nullptr;
  std::unique_ptr<ProjectBoardGanttModel> model_;
  // KDGantt::SummaryHandlingProxyModel (used internally by KDGantt::View) recomputes summary-row
  // spans from their children and writes the result back into the source model via setData() as
  // soon as setModel()/expandAll() run — which would otherwise make LoadFromJson() itself look
  // like a user edit. Set for the duration of LoadFromJson() so EmitDataChanged() can ignore
  // those incidental writes and keep DataChanged meaning "the user changed something".
  bool loading_ = false;

  // Whether the critical-path highlight is currently on -- recomputed and reapplied after every
  // real edit (see EmitDataChanged()) while true, so the highlight doesn't go stale as the user
  // reschedules tasks or changes dependency links.
  bool critical_path_enabled_ = false;
};

}  // namespace cppwiki::gui::project_board::gantt

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_GANTT_PROJECT_BOARD_GANTT_WIDGET_H_
