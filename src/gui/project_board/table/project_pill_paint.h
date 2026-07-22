#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_PILL_PAINT_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_PILL_PAINT_H_

#include <QSize>

class QPainter;
class QRect;
class QString;
class QColor;
class QFontMetrics;

namespace cppwiki::gui::project_board {

// Shared "pill"/tag chip painter used by both ProjectStatusPillDelegate and
// ProjectPriorityPillDelegate — a small rounded, filled, colored chip with centered text, matching
// the intent of the web version's `.project-board-pill` CSS (small rounded background chip, muted
// tone, one color per status/priority). Painting only (no editors/model access) so both delegates
// share exactly one drawing routine instead of two near-duplicate ones.
void PaintPill(QPainter* painter, const QRect& cell_rect, const QString& label,
               const QColor& background, const QColor& foreground);

// The size PaintPill actually needs to draw `label` without clipping it, using the same
// padding/height PaintPill itself uses. Both delegates' sizeHint() call this directly (rather
// than delegating to QStyledItemDelegate::sizeHint(), which measures whatever option.text is —
// and both delegates clear option.text in initStyleOption() so the base implementation would
// otherwise measure an empty string and undersize the column).
QSize PillSizeHint(const QFontMetrics& metrics, const QString& label);

}  // namespace cppwiki::gui::project_board

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_PILL_PAINT_H_
