#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_GANTT_PROJECT_BOARD_GANTT_CRITICAL_PATH_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_GANTT_PROJECT_BOARD_GANTT_CRITICAL_PATH_H_

#include <QJsonObject>
#include <QSet>
#include <QString>

namespace cppwiki::gui::project_board::gantt {

struct CriticalPathResult {
  // Task ids (ProjectTask::id) with zero slack/float -- the chain that determines the project's
  // overall duration. Delaying any of them delays the whole project.
  QSet<QString> critical_task_ids{};
  // Link ids (ProjectLink::id) connecting two critical tasks. A subset of "links whose endpoints
  // are both critical" (see ComputeCriticalPath()'s doc comment for the precision this trades
  // away).
  QSet<QString> critical_link_ids{};
};

// Computes the critical path through `board` (a `{ tasks, columns, links }` document matching the
// schema ProjectBoardGanttModel reads/writes) using the standard Critical Path Method: every task
// is treated as an activity with a duration (its `duration` field, in days; 0 for milestones),
// dependency `links` are precedence constraints between activities (honoring relationType --
// s2s/s2e/e2e/e2s -- and an optional `lag` in days), and the critical path is whichever chain of
// zero-slack activities determines the overall project length (forward pass for earliest
// start/finish, backward pass for latest start/finish, per the textbook algorithm).
//
// This is independent of whatever start/end dates are actually stored on each task -- it answers
// "what does the dependency graph + durations imply", not "what did the calendar end up being".
//
// A link is reported critical only if *both* its endpoints are on the critical path, which is a
// common simplification: it doesn't verify the specific precedence inequality that produced a
// task's schedule was tight, so a link between two critical tasks that aren't actually adjacent on
// the critical chain (e.g. both critical via different, unrelated paths) could be reported as
// critical when it isn't load-bearing. Acceptable for a visual highlight; not a substitute for a
// real CPM tool if that distinction matters.
//
// Returns an empty result (no highlighting) if the dependency graph contains a cycle, since CPM is
// undefined on a graph with cycles.
[[nodiscard]] auto ComputeCriticalPath(const QJsonObject& board) -> CriticalPathResult;

}  // namespace cppwiki::gui::project_board::gantt

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_GANTT_PROJECT_BOARD_GANTT_CRITICAL_PATH_H_
