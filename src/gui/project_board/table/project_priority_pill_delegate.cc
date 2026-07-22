#include "gui/project_board/table/project_priority_pill_delegate.h"

#include <QAbstractItemModel>
#include <QColor>
#include <QComboBox>
#include <QPainter>
#include <QStyleOptionViewItem>

#include "gui/project_board/table/project_pill_paint.h"
#include "gui/project_board/table/project_task.h"
#include "gui/project_board/table/project_task_table_model.h"

namespace cppwiki::gui::project_board {

namespace {

// Matches .project-board-pill--priority-low/medium/high in styles.css.
const QColor kLowBackground(110, 130, 150, 56);
const QColor kLowForeground(0xa9, 0xbc, 0xce);
const QColor kMediumBackground(210, 160, 70, 61);
const QColor kMediumForeground(0xe6, 0xc0, 0x69);
const QColor kHighBackground(210, 90, 90, 61);
const QColor kHighForeground(0xe8, 0x88, 0x88);

void ColorsForPriority(int priority, QColor* background, QColor* foreground) {
  switch (priority) {
    case kPriorityMedium:
      *background = kMediumBackground;
      *foreground = kMediumForeground;
      return;
    case kPriorityHigh:
      *background = kHighBackground;
      *foreground = kHighForeground;
      return;
    case kPriorityLow:
    default:
      *background = kLowBackground;
      *foreground = kLowForeground;
      return;
  }
}

}  // namespace

ProjectPriorityPillDelegate::ProjectPriorityPillDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

void ProjectPriorityPillDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                        const QModelIndex& index) const {
  QStyledItemDelegate::paint(painter, option, index);

  const int tone = index.data(ProjectTaskTableModel::kToneRole).toInt();
  if (tone <= 0) {
    // No priority set (tone == -1) — GridTab's PriorityCell renders nothing in this case too.
    return;
  }
  const QString label = index.data(ProjectTaskTableModel::kPillLabelRole).toString();
  QColor background;
  QColor foreground;
  ColorsForPriority(tone, &background, &foreground);
  PaintPill(painter, option.rect, label, background, foreground);
}

void ProjectPriorityPillDelegate::initStyleOption(QStyleOptionViewItem* option,
                                                  const QModelIndex& index) const {
  QStyledItemDelegate::initStyleOption(option, index);
  option->text.clear();
}

QSize ProjectPriorityPillDelegate::sizeHint(const QStyleOptionViewItem& option,
                                            const QModelIndex& index) const {
  const int tone = index.data(ProjectTaskTableModel::kToneRole).toInt();
  if (tone <= 0) {
    return QStyledItemDelegate::sizeHint(option, index);
  }
  const QString label = index.data(ProjectTaskTableModel::kPillLabelRole).toString();
  return PillSizeHint(option.fontMetrics, label);
}

QWidget* ProjectPriorityPillDelegate::createEditor(QWidget* parent,
                                                   const QStyleOptionViewItem& /*option*/,
                                                   const QModelIndex& /*index*/) const {
  auto* combo = new QComboBox(parent);
  combo->addItem(QStringLiteral("None"), 0);
  combo->addItem(PriorityLabel(kPriorityLow), kPriorityLow);
  combo->addItem(PriorityLabel(kPriorityMedium), kPriorityMedium);
  combo->addItem(PriorityLabel(kPriorityHigh), kPriorityHigh);
  return combo;
}

void ProjectPriorityPillDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
  auto* combo = qobject_cast<QComboBox*>(editor);
  if (combo == nullptr) {
    return;
  }
  const int current_priority = index.data(Qt::EditRole).toInt();
  const int found = combo->findData(current_priority);
  combo->setCurrentIndex(found >= 0 ? found : 0);
}

void ProjectPriorityPillDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                               const QModelIndex& index) const {
  auto* combo = qobject_cast<QComboBox*>(editor);
  if (combo == nullptr) {
    return;
  }
  model->setData(index, combo->currentData().toInt(), Qt::EditRole);
}

void ProjectPriorityPillDelegate::updateEditorGeometry(QWidget* editor,
                                                       const QStyleOptionViewItem& option,
                                                       const QModelIndex& /*index*/) const {
  SizeComboEditorToContents(editor, option);
}

}  // namespace cppwiki::gui::project_board
