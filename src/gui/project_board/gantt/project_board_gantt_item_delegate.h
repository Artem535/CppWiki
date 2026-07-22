#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_GANTT_PROJECT_BOARD_GANTT_ITEM_DELEGATE_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_GANTT_PROJECT_BOARD_GANTT_ITEM_DELEGATE_H_

#include <KDGanttItemDelegate>
#include <QString>

class QPainter;
class QModelIndex;

namespace cppwiki::gui::project_board::gantt {

// KDGantt::ItemDelegate::paintGanttItem() draws every bar type as a plain sharp-cornered
// rectangle (or, for summaries, a pointed bracket shape) with no shadow -- a look that reads as
// dated next to the web Project board's Gantt tab (SVAR react-gantt's WillowDark theme), which
// uses rounded 3px bars with a soft drop shadow. project_board_gantt_widget.cc's theme block
// already recolors bars/text to match that theme's palette; this delegate carries the shape the
// rest of the way so the native replacement doesn't just have the right colors on the wrong
// silhouette. Colors themselves stay wherever the caller set them via setDefaultBrush()/
// setDefaultPen() (inherited, unchanged) -- only the geometry paintGanttItem() draws changes.
//
// Also draws a small connector-dot hint at the left/right edges of Task/Milestone bars (the only
// item types KDGantt::GraphicsItem actually lets you start a dependency drag from -- see
// interactionStateFor()'s State_None for TypeSummary) and overrides toolTip() to explain the
// gesture, since KDGantt::GraphicsItem::mousePressEvent()/mouseMoveEvent() already implement
// interactive link creation (press a bar, drag mostly vertically, release over another bar) with
// zero visual affordance -- without this hint nobody would ever discover it exists.
class ProjectBoardGanttItemDelegate : public KDGantt::ItemDelegate {
  Q_OBJECT

 public:
  explicit ProjectBoardGanttItemDelegate(QObject* parent = nullptr);

  void paintGanttItem(QPainter* painter, const KDGantt::StyleOptionGanttItem& opt,
                      const QModelIndex& idx) override;
  [[nodiscard]] auto toolTip(const QModelIndex& idx) const -> QString override;
};

}  // namespace cppwiki::gui::project_board::gantt

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_GANTT_PROJECT_BOARD_GANTT_ITEM_DELEGATE_H_
