#include "gui/document_tree_item_delegate.h"

#include <algorithm>

#include <QPainter>
#include <QStyleOptionViewItem>
#include <QTreeView>
#include <QWidget>

#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/utils/ImageUtils.hpp>
#include <oclero/qlementine/utils/StateUtils.hpp>

#include "gui/document_tree_model.h"

namespace cppwiki::gui {
namespace {

auto ThemeSpacing(const oclero::qlementine::Theme& theme, int fallback) -> int {
  return theme.spacing > 0 ? theme.spacing : fallback;
}

auto ThemeRadius(const oclero::qlementine::Theme& theme) -> qreal {
  return theme.borderRadius > 0.0 ? theme.borderRadius : 8.0;
}

auto WithAlpha(QColor color, int alpha) -> QColor {
  color.setAlpha(alpha);
  return color;
}

void DrawAddChildButton(QPainter* painter, const QRect& rect, const QColor& foreground) {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);

  const QPoint center = rect.center();
  const int half = std::max(4, std::min(rect.width(), rect.height()) / 5);

  painter->setPen(QPen(foreground, 1.8, Qt::SolidLine, Qt::RoundCap));
  painter->drawLine(QPoint(center.x() - half, center.y()), QPoint(center.x() + half, center.y()));
  painter->drawLine(QPoint(center.x(), center.y() - half), QPoint(center.x(), center.y() + half));

  painter->restore();
}

}  // namespace

DocumentTreeItemDelegate::DocumentTreeItemDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

oclero::qlementine::QlementineStyle* DocumentTreeItemDelegate::getQlementineStyle() const {
  if (const auto* widget = qobject_cast<const QWidget*>(parent())) {
    if (auto* style = qobject_cast<oclero::qlementine::QlementineStyle*>(widget->style())) {
      return style;
    }
  }
  return oclero::qlementine::appStyle();
}

void DocumentTreeItemDelegate::initStyleOption(QStyleOptionViewItem* option,
                                               const QModelIndex& index) const {
  QStyledItemDelegate::initStyleOption(option, index);
  option->decorationSize = QSize(kIconSize, kIconSize);
  option->displayAlignment = Qt::AlignVCenter | Qt::AlignLeft;
}

void DocumentTreeItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                     const QModelIndex& index) const {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);

  QStyleOptionViewItem opt = option;
  initStyleOption(&opt, index);

  // DocumentTreeView intentionally removes State_Selected in drawRow() to stop
  // QTreeView/QStyle from painting the blue selected block in the branch area.
  // Restore selection only for the delegate-owned item rect.
  if (const auto* tree_view = qobject_cast<const QTreeView*>(opt.widget)) {
    if (tree_view->selectionModel() != nullptr && tree_view->selectionModel()->isSelected(index)) {
      opt.state |= QStyle::State_Selected;
    }
  }

  const auto* qlementine_style = getQlementineStyle();
  const auto& theme = qlementine_style ? qlementine_style->theme() : oclero::qlementine::Theme{};

  const auto mouse = oclero::qlementine::getMouseState(opt.state);
  const auto selected = oclero::qlementine::getSelectionState(opt.state);
  const auto focus = oclero::qlementine::getFocusState(opt.state.testFlag(QStyle::State_HasFocus));
  const auto active = oclero::qlementine::getActiveState(opt.state);

  const int margin = ThemeSpacing(theme, 8);
  const int vertical_padding = ThemeSpacing(theme, 8);
  const qreal radius = ThemeRadius(theme);

  const QRect hover_row_rect = opt.rect.adjusted(margin / 2, vertical_padding / 2, -margin / 2,
                                                 -vertical_padding / 2);
  const QRect selected_row_rect =
      opt.rect.adjusted(0, vertical_padding / 2, 0, -vertical_padding / 2);

  QColor background;
  QColor foreground;
  if (qlementine_style) {
    background = qlementine_style->listItemRowBackgroundColor(
        mouse, oclero::qlementine::AlternateState::NotAlternate);
    foreground = qlementine_style->listItemForegroundColor(mouse, selected, focus, active);

    if (opt.state.testFlag(QStyle::State_MouseOver)) {
      background = WithAlpha(theme.neutralColor, 18);
    }
    if (selected == oclero::qlementine::SelectionState::Selected) {
      background = WithAlpha(theme.neutralColor, 32);
      foreground = qlementine_style->listItemForegroundColor(
          oclero::qlementine::MouseState::Normal,
          oclero::qlementine::SelectionState::NotSelected, focus, active);
      if (opt.state.testFlag(QStyle::State_MouseOver)) {
        background = WithAlpha(theme.neutralColor, 40);
      }
    }
  } else {
    background = opt.palette.base().color();
    foreground = opt.palette.text().color();
    if (opt.state.testFlag(QStyle::State_Selected)) {
      background = WithAlpha(opt.palette.mid().color(), 56);
      foreground = opt.palette.text().color();
    } else if (opt.state.testFlag(QStyle::State_MouseOver)) {
      background = opt.palette.alternateBase().color();
    }
  }

  if (selected == oclero::qlementine::SelectionState::Selected ||
      opt.state.testFlag(QStyle::State_MouseOver)) {
    painter->setPen(Qt::NoPen);
    painter->setBrush(background);
    const QRect background_rect =
        selected == oclero::qlementine::SelectionState::Selected ? selected_row_rect : hover_row_rect;
    painter->drawRoundedRect(background_rect, radius, radius);
  }

  QRect content_rect = selected_row_rect.adjusted(margin, 0, -margin, 0);

  const QRect add_child_button_rect = addChildButtonRect(opt, index);
  content_rect = content_rect.adjusted(
      0, 0,
      add_child_button_rect.isValid() ? -(margin + kAddChildButtonSize + margin / 2) : -margin,
      0);

  const QVariant icon_variant = index.data(Qt::DecorationRole);
  const QIcon icon = icon_variant.isValid() ? icon_variant.value<QIcon>() : QIcon{};
  if (!icon.isNull()) {
    const int icon_y = content_rect.top() + (content_rect.height() - kIconSize) / 2;
    const QRect icon_rect(content_rect.left(), icon_y, kIconSize, kIconSize);
    content_rect.adjust(kIconSize + margin / 2, 0, 0, 0);

    const auto pixmap = icon.pixmap(kIconSize, kIconSize);
    if (!pixmap.isNull()) {
      const auto auto_icon_color = qlementine_style
                                       ? qlementine_style->autoIconColor(opt.widget)
                                       : oclero::qlementine::AutoIconColor::None;
      const auto colorized_pixmap = qlementine_style
                                        ? qlementine_style->getColorizedPixmap(
                                              pixmap, auto_icon_color, foreground, theme.secondaryColor)
                                        : pixmap;
      painter->drawPixmap(icon_rect, colorized_pixmap);
    }
  }

  const QString text = index.data(Qt::DisplayRole).toString();
  if (!text.isEmpty()) {
    QFont font = opt.font;
    if (qlementine_style) {
      font = qlementine_style->fontForTextRole(oclero::qlementine::TextRole::Default);
    }
    font.setWeight(QFont::Normal);
    painter->setFont(font);
    painter->setPen(foreground);

    const auto metrics = painter->fontMetrics();
    const int available_text_width = std::max(0, content_rect.width());
    const QString elided = metrics.elidedText(text, Qt::ElideRight, available_text_width);
    painter->drawText(content_rect, Qt::AlignVCenter | Qt::AlignLeft, elided);
  }

  if (add_child_button_rect.isValid()) {
    DrawAddChildButton(painter, add_child_button_rect, foreground);
  }

  painter->restore();
}

QSize DocumentTreeItemDelegate::sizeHint(const QStyleOptionViewItem& option,
                                         const QModelIndex& index) const {
  Q_UNUSED(index)

  const auto* qlementine_style = getQlementineStyle();
  const auto& theme = qlementine_style ? qlementine_style->theme() : oclero::qlementine::Theme{};
  const int spacing = ThemeSpacing(theme, 8);
  const int control_height = theme.controlHeightMedium > 0 ? theme.controlHeightMedium : 36;
  const int height = std::max(control_height, kIconSize + spacing);
  const int width = std::max(160, option.fontMetrics.horizontalAdvance(option.text) + spacing * 4);
  return QSize(width, height);
}

QRect DocumentTreeItemDelegate::addChildButtonRect(const QStyleOptionViewItem& option,
                                                   const QModelIndex& index) const {
  if (!index.isValid() || index.data(DocumentTreeModel::kIsActionRole).toBool() ||
      index.data(DocumentTreeModel::kAddChildActionRole).toBool() ||
      !option.state.testFlag(QStyle::State_MouseOver)) {
    return QRect();
  }

  const auto* qlementine_style = getQlementineStyle();
  const auto& theme = qlementine_style ? qlementine_style->theme() : oclero::qlementine::Theme{};
  const int margin = ThemeSpacing(theme, 8);
  const int vertical_padding = ThemeSpacing(theme, 8);

  const QRect row_rect = option.rect.adjusted(0, vertical_padding / 2, 0, -vertical_padding / 2);
  const int size = std::min(kAddChildButtonSize, std::max(0, row_rect.height()));
  if (size <= 0) {
    return QRect();
  }

  const int button_y = row_rect.top() + (row_rect.height() - size) / 2;
  return QRect(row_rect.right() - margin - size + 1, button_y, size, size);
}

}  // namespace cppwiki::gui
