#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_PILL_PAINT_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_PILL_PAINT_H_

class QPainter;
class QRect;
class QString;
class QColor;

namespace cppwiki::gui::project_board {

// Shared "pill"/tag chip painter used by both ProjectStatusPillDelegate and
// ProjectPriorityPillDelegate — a small rounded, filled, colored chip with centered text, matching
// the intent of the web version's `.project-board-pill` CSS (small rounded background chip, muted
// tone, one color per status/priority). Painting only (no editors/model access) so both delegates
// share exactly one drawing routine instead of two near-duplicate ones.
void PaintPill(QPainter* painter, const QRect& cell_rect, const QString& label,
               const QColor& background, const QColor& foreground);

}  // namespace cppwiki::gui::project_board

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_PILL_PAINT_H_
