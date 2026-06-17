#include "gui/document_tree_view.h"

#include <algorithm>

#include <spdlog/spdlog.h>

#include <QAbstractItemModel>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPolygon>
#include <QStyleOptionViewItem>

#include "gui/document_tree_model.h"

namespace cppwiki::gui {
namespace {

constexpr int kAddChildButtonSize = 24;
constexpr int kRowVerticalPadding = 8;
constexpr int kRowMargin = 8;

void DrawDisclosureTriangle(QPainter* painter, const QRect& rect, bool expanded) {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);

  const QPoint center = rect.center();
  constexpr int kHalf = 4;

  QPolygon triangle;
  if (expanded) {
    triangle << QPoint(center.x() - kHalf, center.y() - 2)
             << QPoint(center.x() + kHalf, center.y() - 2)
             << QPoint(center.x(), center.y() + kHalf);
  } else {
    triangle << QPoint(center.x() - 2, center.y() - kHalf)
             << QPoint(center.x() - 2, center.y() + kHalf)
             << QPoint(center.x() + kHalf, center.y());
  }

  const QColor color = painter->pen().color();
  painter->setPen(Qt::NoPen);
  painter->setBrush(color);
  painter->drawPolygon(triangle);

  painter->restore();
}

}  // namespace

DocumentTreeView::DocumentTreeView(QWidget* parent) : QTreeView(parent) {
  setRootIsDecorated(true);
  setItemsExpandable(true);
  setExpandsOnDoubleClick(true);
  setIndentation(20);

  setMouseTracking(true);
  viewport()->setMouseTracking(true);
  viewport()->setAttribute(Qt::WA_Hover, true);
}

void DocumentTreeView::setModel(QAbstractItemModel* model) {
  if (this->model() != nullptr) {
    disconnect(this->model(), nullptr, this, nullptr);
  }

  clearTrackedIndexes();
  QTreeView::setModel(model);

  if (model == nullptr) {
    return;
  }

  const auto reset_tracking = [this]() {
    clearTrackedIndexes();
    viewport()->update();
  };

  connect(model, &QAbstractItemModel::modelAboutToBeReset, this, reset_tracking);
  connect(model, &QAbstractItemModel::modelReset, this, reset_tracking);
  connect(model, &QAbstractItemModel::layoutAboutToBeChanged, this, reset_tracking);
  connect(model, &QAbstractItemModel::layoutChanged, this, reset_tracking);
  connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this, reset_tracking);
}

void DocumentTreeView::drawBranches(QPainter* painter, const QRect& rect,
                                    const QModelIndex& index) const {
  // Erase the branch area explicitly. QTreeView can paint selected row panels
  // before drawBranches(), so replacing only PE_IndicatorBranch is not enough
  // with some styles.
  painter->fillRect(rect, viewport()->palette().color(QPalette::Base));

  if (!index.isValid() || model() == nullptr || !model()->hasChildren(index)) {
    return;
  }

  // Do not call QTreeView::drawBranches(). Some styles paint selected/hover
  // background in the branch area there, which creates the blue square to the
  // left of the row. The view still uses the native expansion logic; only the
  // branch painting is custom.
  const QPen old_pen = painter->pen();
  QPen pen = old_pen;
  pen.setColor(palette().color(QPalette::Mid));
  painter->setPen(pen);

  DrawDisclosureTriangle(painter, rect, isExpanded(index));

  painter->setPen(old_pen);
}

void DocumentTreeView::drawRow(QPainter* painter, const QStyleOptionViewItem& option,
                               const QModelIndex& index) const {
  // QTreeView/QStyle may paint the selected row background across the whole row,
  // including the branch/indent area. We clear the selected/focus flags here so
  // the view does not paint that left block. The delegate restores the selected
  // state for the actual item rect by checking selectionModel()->isSelected().
  QStyleOptionViewItem row_option(option);
  row_option.state &= ~QStyle::State_Selected;
  row_option.state &= ~QStyle::State_HasFocus;

  QTreeView::drawRow(painter, row_option, index);
}

void DocumentTreeView::mouseMoveEvent(QMouseEvent* event) {
  const QModelIndex previous_hovered = hovered_index_;
  hovered_index_ = indexAt(event->pos());

  if (previous_hovered != hovered_index_) {
    if (previous_hovered.isValid()) {
      viewport()->update(visualRect(previous_hovered));
    }
    if (hovered_index_.isValid()) {
      viewport()->update(visualRect(hovered_index_));
    }
  }

  QTreeView::mouseMoveEvent(event);
}

void DocumentTreeView::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    const QModelIndex index = indexAt(event->pos());
    if (index.isValid() && addChildButtonRect(index).contains(event->pos())) {
      pressed_add_child_index_ = index;
      spdlog::info("Add child button pressed");
      event->accept();
      return;
    }
  }

  pressed_add_child_index_ = QPersistentModelIndex();
  QTreeView::mousePressEvent(event);
}

void DocumentTreeView::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton && pressed_add_child_index_.isValid()) {
    const QModelIndex pressed_index = pressed_add_child_index_;
    const bool still_inside_button = addChildButtonRect(pressed_index).contains(event->pos());

    pressed_add_child_index_ = QPersistentModelIndex();

    if (still_inside_button) {
      spdlog::info("Add child button triggered");
      emit addChildRequested(pressed_index);
      event->accept();
      return;
    }

    spdlog::info("Add child button release ignored");
  }

  pressed_add_child_index_ = QPersistentModelIndex();
  QTreeView::mouseReleaseEvent(event);
}

void DocumentTreeView::leaveEvent(QEvent* event) {
  const QModelIndex previous_hovered = hovered_index_;
  clearTrackedIndexes();

  if (previous_hovered.isValid()) {
    viewport()->update(visualRect(previous_hovered));
  }

  QTreeView::leaveEvent(event);
}

void DocumentTreeView::clearTrackedIndexes() {
  hovered_index_ = QModelIndex();
  pressed_add_child_index_ = QModelIndex();
}

QRect DocumentTreeView::addChildButtonRect(const QModelIndex& index) const {
  if (!canShowAddChildButton(index)) {
    return QRect();
  }

  const QRect row_rect = visualRect(index);
  if (!row_rect.isValid()) {
    return QRect();
  }

  const QRect content_rect = row_rect.adjusted(0, kRowVerticalPadding / 2, 0, -kRowVerticalPadding / 2);
  const int size = std::min(kAddChildButtonSize, std::max(0, content_rect.height()));
  if (size <= 0) {
    return QRect();
  }

  const int x = content_rect.right() - kRowMargin - size + 1;
  const int y = content_rect.top() + (content_rect.height() - size) / 2;
  return QRect(x, y, size, size);
}

bool DocumentTreeView::canShowAddChildButton(const QModelIndex& index) const {
  if (!index.isValid()) {
    return false;
  }

  if (index.data(DocumentTreeModel::kIsActionRole).toBool()) {
    return false;
  }

  if (index.data(DocumentTreeModel::kAddChildActionRole).toBool()) {
    return false;
  }

  return true;
}

}  // namespace cppwiki::gui
