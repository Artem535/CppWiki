#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_TASK_TABLE_WIDGET_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_TASK_TABLE_WIDGET_H_

#include <QJsonArray>
#include <QWidget>

#include "gui/project_board/table/project_task.h"

class QTableView;

namespace cppwiki::gui::project_board {

class ProjectTaskTableModel;

// A self-contained native replacement for the web Table (Grid) view of the Project board document
// kind (see frontend/editor/src/project/ProjectBoardView.tsx's GridTab) — a QTableView over
// ProjectTaskTableModel with the pill/date/spin-box delegates wired in and sorting enabled.
//
// Standalone: this widget has no knowledge of DocumentKind, IPage, or the sync/bridge layer — it
// only knows the ProjectBoardDocument shape (see project_task.h) and is not yet wired into
// MainWindow/Page. See demo_main.cc for a small runnable demonstration.
class ProjectTaskTableWidget : public QWidget {
  Q_OBJECT

 public:
  explicit ProjectTaskTableWidget(QWidget* parent = nullptr);

  // Replaces the currently displayed document wholesale.
  void setDocument(const ProjectBoardDocument& document);
  // The document's current state, including any edits made through the view. `links` is carried
  // through unchanged from the last setDocument() call (this view neither reads nor edits it).
  [[nodiscard]] ProjectBoardDocument document() const;

  [[nodiscard]] ProjectTaskTableModel* model() const {
    return model_;
  }
  [[nodiscard]] QTableView* view() const {
    return view_;
  }

 signals:
  // Emitted whenever an edit changes the underlying task data — callers that want to persist the
  // document (once this is wired into the app) can listen for this instead of polling document().
  void documentEdited();

 private:
  ProjectTaskTableModel* model_;
  QTableView* view_;
  QJsonArray links_;
};

}  // namespace cppwiki::gui::project_board

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_TASK_TABLE_WIDGET_H_
