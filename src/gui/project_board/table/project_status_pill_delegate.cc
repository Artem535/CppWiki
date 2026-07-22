#include "gui/project_board/table/project_status_pill_delegate.h"

#include <QAbstractItemModel>
#include <QColor>
#include <QComboBox>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <algorithm>
#include <iterator>

#include "gui/project_board/table/project_pill_paint.h"
#include "gui/project_board/table/project_task_table_model.h"

namespace cppwiki::gui::project_board {

namespace {

// Matches .project-board-pill--tone-0..5 in frontend/editor/src/styles.css (rgb(...) / 22%
// backgrounds over a dark app theme, paired foreground colors) — six tones cycling by the
// status's position in the board's column list (see ProjectTaskTableModel::statusToneForColumnId).
struct ToneColors {
  QColor background;
  QColor foreground;
};

const ToneColors kTonePalette[] = {
    {QColor(85, 130, 200, 56), QColor(0x8f, 0xb8, 0xec)},
    {QColor(120, 170, 110, 56), QColor(0x9b, 0xcf, 0x8d)},
    {QColor(200, 150, 80, 56), QColor(0xe0, 0xb8, 0x77)},
    {QColor(170, 110, 190, 56), QColor(0xcc, 0x9e, 0xe3)},
    {QColor(200, 100, 110, 56), QColor(0xe5, 0x9a, 0xa2)},
    {QColor(90, 170, 180, 56), QColor(0x8f, 0xd4, 0xdd)},
};
constexpr int kTonePaletteSize = static_cast<int>(std::size(kTonePalette));

// Matches .project-board-pill--unknown: a muted, neutral tone for a task pointing at a status
// column that no longer exists.
const ToneColors kUnknownTone{QColor(128, 128, 128, 40), QColor(160, 160, 160)};

const ToneColors& ToneColorsFor(int tone) {
  if (tone < 0 || tone >= kTonePaletteSize) {
    return kUnknownTone;
  }
  return kTonePalette[tone];
}

}  // namespace

ProjectStatusPillDelegate::ProjectStatusPillDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

void ProjectStatusPillDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const {
  QStyleOptionViewItem plain_option(option);
  plain_option.text.clear();
  QStyledItemDelegate::paint(painter, plain_option, index);

  const QString label = index.data(ProjectTaskTableModel::kPillLabelRole).toString();
  const int tone = index.data(ProjectTaskTableModel::kToneRole).toInt();
  const ToneColors& colors = ToneColorsFor(tone);
  PaintPill(painter, option.rect, label, colors.background, colors.foreground);
}

QSize ProjectStatusPillDelegate::sizeHint(const QStyleOptionViewItem& option,
                                          const QModelIndex& index) const {
  QSize hint = QStyledItemDelegate::sizeHint(option, index);
  hint.setHeight(std::max(hint.height(), 30));
  return hint;
}

QWidget* ProjectStatusPillDelegate::createEditor(QWidget* parent,
                                                 const QStyleOptionViewItem& /*option*/,
                                                 const QModelIndex& index) const {
  auto* combo = new QComboBox(parent);
  const auto* model = qobject_cast<const ProjectTaskTableModel*>(index.model());
  if (model != nullptr) {
    for (const ProjectColumn& column : model->boardColumns()) {
      combo->addItem(column.label, column.id);
    }
  }
  return combo;
}

void ProjectStatusPillDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
  auto* combo = qobject_cast<QComboBox*>(editor);
  if (combo == nullptr) {
    return;
  }
  const QString current_id = index.data(Qt::EditRole).toString();
  const int found = combo->findData(current_id);
  combo->setCurrentIndex(found >= 0 ? found : 0);
}

void ProjectStatusPillDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                             const QModelIndex& index) const {
  auto* combo = qobject_cast<QComboBox*>(editor);
  if (combo == nullptr) {
    return;
  }
  model->setData(index, combo->currentData().toString(), Qt::EditRole);
}

void ProjectStatusPillDelegate::updateEditorGeometry(QWidget* editor,
                                                     const QStyleOptionViewItem& option,
                                                     const QModelIndex& /*index*/) const {
  editor->setGeometry(option.rect);
}

}  // namespace cppwiki::gui::project_board
