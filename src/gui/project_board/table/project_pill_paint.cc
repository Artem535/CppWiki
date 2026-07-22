#include "gui/project_board/table/project_pill_paint.h"

#include <QColor>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QRect>
#include <QString>
#include <QStyleOptionViewItem>
#include <QWidget>
#include <algorithm>

// QSize is included via the header (project_pill_paint.h).

namespace cppwiki::gui::project_board {

namespace {

constexpr int kHorizontalPadding = 10;
constexpr int kPillHeight = 22;

}  // namespace

QSize PillSizeHint(const QFontMetrics& metrics, const QString& label) {
  if (label.isEmpty()) {
    return QSize(0, kPillHeight + 8);
  }
  const int text_width = metrics.horizontalAdvance(label);
  return QSize(text_width + 2 * kHorizontalPadding + 4, kPillHeight + 8);
}

void PaintPill(QPainter* painter, const QRect& cell_rect, const QString& label,
               const QColor& background, const QColor& foreground) {
  if (label.isEmpty()) {
    return;
  }

  const QFontMetrics metrics(painter->font());
  const int text_width = metrics.horizontalAdvance(label);
  const int pill_width = std::min(cell_rect.width() - 4, text_width + 2 * kHorizontalPadding);
  if (pill_width <= 0) {
    return;
  }

  QRect pill_rect(0, 0, pill_width, kPillHeight);
  pill_rect.moveCenter(cell_rect.center());
  // Clamp back inside the cell horizontally in case the ideal centered pill would overhang a
  // narrow column.
  if (pill_rect.left() < cell_rect.left()) {
    pill_rect.moveLeft(cell_rect.left() + 2);
  }

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);

  QPainterPath path;
  path.addRoundedRect(pill_rect, pill_rect.height() / 2.0, pill_rect.height() / 2.0);
  painter->fillPath(path, background);

  painter->setPen(foreground);
  painter->drawText(pill_rect, Qt::AlignCenter, label);

  painter->restore();
}

void SizeComboEditorToContents(QWidget* editor, const QStyleOptionViewItem& option) {
  QRect rect = option.rect;
  const int preferred_width = editor->sizeHint().width();
  if (preferred_width > rect.width()) {
    rect.setWidth(preferred_width);
  }
  if (const QWidget* parent = editor->parentWidget()) {
    const int max_right = parent->rect().right();
    if (rect.right() > max_right) {
      rect.moveRight(max_right);
    }
  }
  editor->setGeometry(rect);
}

}  // namespace cppwiki::gui::project_board
