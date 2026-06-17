#include "gui/document_tree_view.h"

#include <algorithm>

#include <QAbstractItemDelegate>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPolygon>
#include <QRect>
#include <QStyleOptionViewItem>

#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/utils/StateUtils.hpp>

#include "gui/document_tree_model.h"

namespace cppwiki::gui {
namespace {

void DrawDisclosureTriangle(QPainter* painter, const QRect& branch_rect, int indentation,
                            bool expanded, const QColor& color) {
  if (painter == nullptr || !branch_rect.isValid()) {
    return;
  }

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);

  const int indicator_span = indentation > 0 ? indentation : branch_rect.width();
  const int center_x = branch_rect.right() - indicator_span / 2;
  const QPoint center(center_x, branch_rect.center().y());

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

  painter->setPen(Qt::NoPen);
  painter->setBrush(color);
  painter->drawPolygon(triangle);
  painter->restore();
}

oclero::qlementine::QlementineStyle* GetQlementineStyle(const QWidget* widget) {
  if (widget != nullptr) {
    if (auto* style = qobject_cast<oclero::qlementine::QlementineStyle*>(widget->style())) {
      return style;
    }
  }

  return oclero::qlementine::appStyle();
}

QColor RowBackgroundColor(const QTreeView* view, const QModelIndex& index, bool hovered) {
  if (view == nullptr) {
    return QColor{};
  }

  const bool selected = view->selectionModel() != nullptr && view->selectionModel()->isSelected(index);
  if (!selected && !hovered) {
    return QColor{};
  }

  if (auto* style = GetQlementineStyle(view)) {
    const auto focus = view->hasFocus() ? oclero::qlementine::FocusState::Focused
                                        : oclero::qlementine::FocusState::NotFocused;
    const auto active = view->isActiveWindow() ? oclero::qlementine::ActiveState::Active
                                               : oclero::qlementine::ActiveState::NotActive;

    // Use a neutral hovered background even for selected rows. This avoids the
    // native blue selection rectangle and matches the compact sidebar look.
    return style->listItemBackgroundColor(oclero::qlementine::MouseState::Hovered,
                                          oclero::qlementine::SelectionState::NotSelected,
                                          focus, active, index, view);
  }

  return view->palette().color(QPalette::AlternateBase);
}

QColor BranchArrowColor(const QTreeView* view, const QModelIndex& index, bool hovered) {
  if (view == nullptr) {
    return QColor{};
  }

  const bool selected = view->selectionModel() != nullptr && view->selectionModel()->isSelected(index);

  if (auto* style = GetQlementineStyle(view)) {
    const auto mouse = hovered ? oclero::qlementine::MouseState::Hovered
                               : oclero::qlementine::MouseState::Normal;
    const auto selection = selected ? oclero::qlementine::SelectionState::Selected
                                    : oclero::qlementine::SelectionState::NotSelected;
    const auto focus = view->hasFocus() ? oclero::qlementine::FocusState::Focused
                                        : oclero::qlementine::FocusState::NotFocused;
    const auto active = view->isActiveWindow() ? oclero::qlementine::ActiveState::Active
                                               : oclero::qlementine::ActiveState::NotActive;
    return style->listItemForegroundColor(mouse, selection, focus, active);
  }

  return selected || hovered ? view->palette().color(QPalette::Text)
                             : view->palette().color(QPalette::Mid);
}


void DrawFullRowBackground(QPainter* painter, const QTreeView* view, const QRect& row_source_rect,
                           const QColor& background) {
  if (painter == nullptr || view == nullptr || !row_source_rect.isValid() ||
      !background.isValid() || background.alpha() <= 0) {
    return;
  }

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);

  constexpr int kHorizontalInset = 0;
  constexpr int kVerticalInset = 2;
  constexpr qreal kRadius = 4.0;

  QRect row_rect(view->viewport()->rect().left() + kHorizontalInset,
                 row_source_rect.top() + kVerticalInset,
                 view->viewport()->rect().width() - kHorizontalInset * 2,
                 row_source_rect.height() - kVerticalInset * 2);

  painter->setPen(Qt::NoPen);
  painter->setBrush(background);
  painter->drawRoundedRect(row_rect, kRadius, kRadius);
  painter->restore();
}

}  // namespace

DocumentTreeView::DocumentTreeView(QWidget* parent) : QTreeView(parent) {
  setRootIsDecorated(true);
  setItemsExpandable(true);
  setExpandsOnDoubleClick(true);
  setIndentation(24);

  setHeaderHidden(true);
  setUniformRowHeights(true);
  setAlternatingRowColors(false);
  setAnimated(true);

  setSelectionMode(QAbstractItemView::SingleSelection);
  setSelectionBehavior(QAbstractItemView::SelectRows);
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  setFocusPolicy(Qt::StrongFocus);

  setMouseTracking(true);
  viewport()->setMouseTracking(true);
  viewport()->setAttribute(Qt::WA_Hover, true);

  setDragEnabled(true);
  setAcceptDrops(true);
  setDropIndicatorShown(true);
  setDefaultDropAction(Qt::MoveAction);
  setDragDropMode(QAbstractItemView::InternalMove);
  setDragDropOverwriteMode(false);
}

void DocumentTreeView::setModel(QAbstractItemModel* model) {
  if (this->model() != nullptr) {
    disconnect(this->model(), nullptr, this, nullptr);
  }

  hovered_index_ = QModelIndex();
  resetPressedAddChildIndex();
  QTreeView::setModel(model);

  if (model == nullptr) {
    return;
  }

  const auto reset_state = [this]() {
    hovered_index_ = QModelIndex();
    resetPressedAddChildIndex();
    viewport()->unsetCursor();
    viewport()->update();
  };

  connect(model, &QAbstractItemModel::modelAboutToBeReset, this, reset_state);
  connect(model, &QAbstractItemModel::modelReset, this, reset_state);
  connect(model, &QAbstractItemModel::layoutAboutToBeChanged, this, reset_state);
  connect(model, &QAbstractItemModel::layoutChanged, this, reset_state);
  connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this, reset_state);
}

void DocumentTreeView::drawBranches(QPainter* painter, const QRect& rect,
                                    const QModelIndex& index) const {
  if (painter == nullptr || !rect.isValid()) {
    return;
  }

  // QTreeView paints the branch / indentation area separately from the item
  // delegate. If we leave this area untouched, the platform style may keep a
  // native blue selected rectangle there. Paint the same row background here,
  // clipped by Qt to the branch area, so the hover/selected pill stays
  // continuous across indentation + item content.
  const bool hovered = index == hovered_index_;
  const QColor background = RowBackgroundColor(this, index, hovered);
  if (background.isValid() && background.alpha() > 0) {
    DrawFullRowBackground(painter, this, rect, background);
  } else {
    painter->fillRect(rect, viewport()->palette().color(QPalette::Base));
  }

  if (!index.isValid() || model() == nullptr || !model()->hasChildren(index)) {
    return;
  }

  DrawDisclosureTriangle(painter, rect, indentation(), isExpanded(index),
                         BranchArrowColor(this, index, hovered));
}

void DocumentTreeView::drawRow(QPainter* painter, const QStyleOptionViewItem& option,
                               const QModelIndex& index) const {
  if (painter == nullptr || !index.isValid()) {
    return;
  }

  const bool hovered = index == hovered_index_;
  const QColor background = RowBackgroundColor(this, index, hovered);
  DrawFullRowBackground(painter, this, option.rect, background);

  // Do not call QTreeView::drawRow() here. Some platform styles paint the
  // current/focus/selection frame inside the branch area after delegate
  // painting, which is exactly the blue rectangle we are trying to remove.
  // We keep QTreeView responsible for branches via drawBranches(), but paint
  // the item content directly through the delegate.
  QStyleOptionViewItem item_option(option);

  // QTreeView::drawRow() receives a row-wide option.rect. When we bypass
  // QTreeView::drawRow() and call the delegate ourselves, we must restore the
  // real item rectangle. visualRect(index) already includes QTreeView
  // indentation/branch offset, so the delegate paints the icon and text at the
  // correct tree level while the hover background remains full-row.
  const QRect item_rect = visualRect(index);
  if (!item_rect.isValid()) {
    return;
  }
  item_option.rect = item_rect;

  item_option.state &= ~QStyle::State_Selected;
  item_option.state &= ~QStyle::State_MouseOver;
  item_option.state &= ~QStyle::State_HasFocus;
  item_option.state &= ~QStyle::State_Sunken;

  if (auto* delegate = itemDelegateForIndex(index)) {
    delegate->paint(painter, item_option, index);
  }
}

void DocumentTreeView::mouseMoveEvent(QMouseEvent* event) {
  const QModelIndex index = indexAt(event->pos());
  setHoveredIndex(index);

  if (index.isValid() && addChildButtonRect(index).contains(event->pos())) {
    viewport()->setCursor(Qt::PointingHandCursor);
  } else {
    viewport()->unsetCursor();
  }

  QTreeView::mouseMoveEvent(event);
}

void DocumentTreeView::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    const QModelIndex index = indexAt(event->pos());
    if (index.isValid() && addChildButtonRect(index).contains(event->pos())) {
      pressed_add_child_index_ = QPersistentModelIndex(index);
      event->accept();
      return;
    }
  }

  resetPressedAddChildIndex();
  QTreeView::mousePressEvent(event);
}

void DocumentTreeView::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton && pressed_add_child_index_.isValid()) {
    const QModelIndex pressed_index = QModelIndex(pressed_add_child_index_);
    const bool released_on_same_button =
        indexAt(event->pos()) == pressed_index && addChildButtonRect(pressed_index).contains(event->pos());

    resetPressedAddChildIndex();

    if (released_on_same_button) {
      emit addChildRequested(pressed_index);
      event->accept();
      return;
    }
  }

  resetPressedAddChildIndex();
  QTreeView::mouseReleaseEvent(event);
}

void DocumentTreeView::leaveEvent(QEvent* event) {
  setHoveredIndex(QModelIndex());
  resetPressedAddChildIndex();
  viewport()->unsetCursor();
  QTreeView::leaveEvent(event);
}

QRect DocumentTreeView::addChildButtonRect(const QModelIndex& index) const {
  if (!canShowAddChildButton(index)) {
    return QRect();
  }

  const QRect row_rect = visualRect(index);
  if (!row_rect.isValid()) {
    return QRect();
  }

  const QRect content_rect = row_rect.adjusted(
      0, kRowVerticalPadding / 2, 0, -kRowVerticalPadding / 2);
  const int size = std::min(kAddChildButtonSize, std::max(0, content_rect.height()));
  if (size <= 0) {
    return QRect();
  }

  const int x = content_rect.right() - kRowHorizontalMargin - size + 1;
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

void DocumentTreeView::resetPressedAddChildIndex() {
  pressed_add_child_index_ = QPersistentModelIndex();
}

void DocumentTreeView::setHoveredIndex(const QModelIndex& index) {
  if (hovered_index_ == index) {
    return;
  }

  const QModelIndex previous = hovered_index_;
  hovered_index_ = index;

  updateRow(previous);
  updateRow(hovered_index_);
}

void DocumentTreeView::updateRow(const QModelIndex& index) const {
  if (!index.isValid()) {
    return;
  }

  QRect rect = visualRect(index);
  if (!rect.isValid()) {
    return;
  }

  rect.setLeft(viewport()->rect().left());
  rect.setRight(viewport()->rect().right());
  viewport()->update(rect.adjusted(0, -2, 0, 2));
}

}  // namespace cppwiki::gui
