#include "gui/project_board/table/project_numeric_spin_delegate.h"

#include <QAbstractItemModel>
#include <QSpinBox>
#include <utility>

namespace cppwiki::gui::project_board {

ProjectNumericSpinDelegate::ProjectNumericSpinDelegate(int minimum, int maximum, QString suffix,
                                                       QObject* parent)
    : QStyledItemDelegate(parent),
      minimum_(minimum),
      maximum_(maximum),
      suffix_(std::move(suffix)) {}

QWidget* ProjectNumericSpinDelegate::createEditor(QWidget* parent,
                                                  const QStyleOptionViewItem& /*option*/,
                                                  const QModelIndex& /*index*/) const {
  auto* spin_box = new QSpinBox(parent);
  spin_box->setRange(minimum_, maximum_);
  spin_box->setSuffix(suffix_);
  return spin_box;
}

void ProjectNumericSpinDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
  auto* spin_box = qobject_cast<QSpinBox*>(editor);
  if (spin_box == nullptr) {
    return;
  }
  spin_box->setValue(index.data(Qt::EditRole).toInt());
}

void ProjectNumericSpinDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                              const QModelIndex& index) const {
  auto* spin_box = qobject_cast<QSpinBox*>(editor);
  if (spin_box == nullptr) {
    return;
  }
  model->setData(index, spin_box->value(), Qt::EditRole);
}

void ProjectNumericSpinDelegate::updateEditorGeometry(QWidget* editor,
                                                      const QStyleOptionViewItem& option,
                                                      const QModelIndex& /*index*/) const {
  editor->setGeometry(option.rect);
}

}  // namespace cppwiki::gui::project_board
