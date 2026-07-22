#include "gui/project_board/gantt/project_board_gantt_item_delegate.h"

#include <KDGanttStyleOptionGanttItem>
#include <QAbstractItemModel>
#include <QModelIndex>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QStyle>

#include "gui/project_board/gantt/project_board_gantt_model.h"

namespace cppwiki::gui::project_board::gantt {

namespace {

// Matches --wx-gantt-bar-border-radius / --wx-gantt-milestone-border-radius (3px) in
// @svar-ui/react-gantt's WillowDark theme.
constexpr qreal kBarRadius = 3.0;

// Bright, warm accent for the critical-path highlight outline -- deliberately far from every
// existing bar fill color (task blue, summary teal, milestone purple, see
// project_board_gantt_widget.cc's theme block) so it reads as "attention" rather than blending
// into the WillowDark palette the rest of the chart uses.
constexpr QColor kCriticalPathColor(0xff, 0x5a, 0x36);
constexpr qreal kCriticalPathOutlineWidth = 2.5;

// Approximates --wx-gantt-bar-shadow's two layers (0px 1px 2px rgba(44,47,60,.06), 0px 3px 10px
// rgba(44,47,60,.12)) as two offset, low-opacity copies of the same shape painted underneath --
// QPainter has no blur primitive, so this is a cheap stand-in rather than a true Gaussian blur.
constexpr QColor kShadowColorFar(44, 47, 60, 31);   // ~0.12 alpha, larger offset
constexpr QColor kShadowColorNear(44, 47, 60, 15);  // ~0.06 alpha, smaller offset

void PaintShadow(QPainter* painter, const QPainterPath& shape) {
  painter->setPen(Qt::NoPen);
  painter->setBrush(kShadowColorFar);
  painter->save();
  painter->translate(0, 3);
  painter->drawPath(shape);
  painter->restore();
  painter->setBrush(kShadowColorNear);
  painter->save();
  painter->translate(0, 1);
  painter->drawPath(shape);
  painter->restore();
}

constexpr qreal kConnectorDotRadius = 3.5;

// KDGantt::GraphicsItem already lets you start a dependency-link drag from a Task or Milestone
// bar (press, then drag mostly vertically -- see GraphicsItem::mouseMoveEvent()) with zero visual
// affordance: no connector handles, no cursor change, nothing. A small dot at each end of the bar
// is a cheap hint that "something can be grabbed here", without needing to touch KDGantt's own
// (private, un-overridable from here) mouse-event handling to make the gesture itself discoverable
// beyond the toolTip() override below.
void PaintConnectorDot(QPainter* painter, const QPointF& center) {
  painter->save();
  painter->setPen(QPen(QColor(0xff, 0xff, 0xff, 0x66), 1.0));
  painter->setBrush(QColor(0xff, 0xff, 0xff, 0x33));
  painter->drawEllipse(center, kConnectorDotRadius, kConnectorDotRadius);
  painter->restore();
}

}  // namespace

ProjectBoardGanttItemDelegate::ProjectBoardGanttItemDelegate(QObject* parent)
    : KDGantt::ItemDelegate(parent) {}

void ProjectBoardGanttItemDelegate::paintGanttItem(QPainter* painter,
                                                   const KDGantt::StyleOptionGanttItem& opt,
                                                   const QModelIndex& idx) {
  if (!idx.isValid()) {
    return;
  }
  const auto type =
      static_cast<KDGantt::ItemType>(idx.model()->data(idx, KDGantt::ItemTypeRole).toInt());
  const QRectF& item_rect = opt.itemRect;

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);

  QPen pen = defaultPen(type);
  if (opt.state & QStyle::State_Selected) {
    pen.setWidth(2 * pen.width());
  }
  const bool is_critical =
      idx.model()->data(idx, ProjectBoardGanttModel::kTaskCriticalPathRole).toBool();

  bool draw_text = true;
  switch (type) {
    case KDGantt::TypeTask:
    case KDGantt::TypeSummary: {
      if (item_rect.isValid()) {
        QRectF bar_rect = item_rect;
        // Summaries (epics/groups) are drawn thinner than plain tasks so a row that has both
        // (a group bar with child task bars visible in nested rows) reads as "this spans its
        // children" rather than as just another same-weight task bar.
        const qreal vertical_fraction = type == KDGantt::TypeSummary ? 0.5 : 0.7;
        const qreal inset = bar_rect.height() * (1.0 - vertical_fraction) / 2.0;
        bar_rect.adjust(0, inset, 0, -inset);

        QPainterPath bar_path;
        bar_path.addRoundedRect(bar_rect, kBarRadius, kBarRadius);
        PaintShadow(painter, bar_path);

        painter->setPen(pen);
        painter->setBrush(defaultBrush(type));
        painter->drawPath(bar_path);

        bool completion_ok = false;
        const qreal completion =
            idx.model()->data(idx, KDGantt::TaskCompletionRole).toReal(&completion_ok);
        if (completion_ok && completion > 0) {
          QRectF progress_rect = bar_rect;
          progress_rect.setWidth(bar_rect.width() * qBound(0.0, completion / 100.0, 1.0));
          QColor progress_color = defaultBrush(type).color().lighter(135);

          painter->save();
          painter->setClipPath(bar_path);
          painter->fillRect(progress_rect, progress_color);
          painter->restore();
        }

        if (is_critical) {
          painter->save();
          painter->setPen(QPen(kCriticalPathColor, kCriticalPathOutlineWidth));
          painter->setBrush(Qt::NoBrush);
          painter->drawPath(bar_path);
          painter->restore();
        }

        // Only plain tasks can start/end a dependency drag (interactionStateFor() returns
        // State_None for TypeSummary, so a summary bar is neither move/resize/link-draggable) --
        // no point hinting at a gesture that won't actually work here.
        if (type == KDGantt::TypeTask) {
          const qreal mid_y = bar_rect.center().y();
          PaintConnectorDot(painter, QPointF(bar_rect.left(), mid_y));
          PaintConnectorDot(painter, QPointF(bar_rect.right(), mid_y));
        }
      }
      break;
    }
    case KDGantt::TypeEvent: {
      if (opt.boundingRect.isValid()) {
        const qreal half = item_rect.height() / 2.0;
        const QPointF center = item_rect.topLeft() + QPointF(0, half);
        QPolygonF diamond;
        diamond << QPointF(center.x(), center.y() - half) << QPointF(center.x() + half, center.y())
                << QPointF(center.x(), center.y() + half) << QPointF(center.x() - half, center.y());
        QPainterPath diamond_path;
        diamond_path.addPolygon(diamond);
        diamond_path.closeSubpath();

        PaintShadow(painter, diamond_path);
        painter->setPen(pen);
        painter->setBrush(defaultBrush(type));
        painter->drawPath(diamond_path);

        if (is_critical) {
          painter->save();
          painter->setPen(QPen(kCriticalPathColor, kCriticalPathOutlineWidth));
          painter->setBrush(Qt::NoBrush);
          painter->drawPath(diamond_path);
          painter->restore();
        }

        PaintConnectorDot(painter, QPointF(center.x() - half, center.y()));
        PaintConnectorDot(painter, QPointF(center.x() + half, center.y()));
      }
      break;
    }
    default:
      draw_text = false;
      break;
  }

  Qt::Alignment alignment;
  switch (opt.displayPosition) {
    case KDGantt::StyleOptionGanttItem::Left:
      alignment = Qt::AlignLeft;
      break;
    case KDGantt::StyleOptionGanttItem::Right:
      alignment = Qt::AlignRight;
      break;
    case KDGantt::StyleOptionGanttItem::Center:
      alignment = Qt::AlignCenter;
      break;
    case KDGantt::StyleOptionGanttItem::Hidden:
      draw_text = false;
      break;
  }
  if (draw_text) {
    QRectF bounding_rect = opt.boundingRect;
    bounding_rect.setY(item_rect.y());
    bounding_rect.setHeight(item_rect.height());
    painter->setPen(defaultPen(type));
    painter->drawText(bounding_rect, static_cast<int>(alignment | Qt::AlignVCenter), opt.text);
  }

  painter->restore();
}

auto ProjectBoardGanttItemDelegate::itemBoundingSpan(const KDGantt::StyleOptionGanttItem& opt,
                                                     const QModelIndex& idx) const
    -> KDGantt::Span {
  const KDGantt::Span base_span = KDGantt::ItemDelegate::itemBoundingSpan(opt, idx);
  if (!idx.isValid()) {
    return base_span;
  }
  const auto type =
      static_cast<KDGantt::ItemType>(idx.model()->data(idx, KDGantt::ItemTypeRole).toInt());
  if (type != KDGantt::TypeTask && type != KDGantt::TypeEvent) {
    return base_span;
  }
  // paintGanttItem() draws a connector dot (radius kConnectorDotRadius, 1px pen) centered exactly
  // on the bar's left/right edges for these two types, so it extends kConnectorDotRadius + 1px
  // past base_span on both sides. GraphicsItem::updateItemFromMouse() only ever setPos()'s the
  // item during a plain drag -- it never re-queries this method or recomputes boundingRect() --
  // so whatever span is returned here is exactly the region Qt invalidates each frame; anything
  // painted outside it is left un-erased at every historical drag position (the smear reported
  // as a bug).
  constexpr qreal kOverflow = kConnectorDotRadius + 1.0;
  return KDGantt::Span(base_span.start() - kOverflow, base_span.length() + 2.0 * kOverflow);
}

auto ProjectBoardGanttItemDelegate::toolTip(const QModelIndex& idx) const -> QString {
  const QString base_tip = KDGantt::ItemDelegate::toolTip(idx);
  if (!idx.isValid()) {
    return base_tip;
  }
  const auto type =
      static_cast<KDGantt::ItemType>(idx.model()->data(idx, KDGantt::ItemTypeRole).toInt());
  if (type != KDGantt::TypeTask && type != KDGantt::TypeEvent) {
    return base_tip;
  }
  const QString hint =
      tr("Drag up or down, then drop on another bar to link them as a dependency.");
  return base_tip.isEmpty() ? hint : base_tip + QLatin1Char('\n') + hint;
}

}  // namespace cppwiki::gui::project_board::gantt
