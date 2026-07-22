#include "gui/project_board/table/project_date_edit_delegate.h"

#include <QAbstractItemModel>
#include <QDate>
#include <QDateEdit>

namespace cppwiki::gui::project_board {

ProjectDateEditDelegate::ProjectDateEditDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QWidget* ProjectDateEditDelegate::createEditor(QWidget* parent,
                                               const QStyleOptionViewItem& /*option*/,
                                               const QModelIndex& /*index*/) const {
  auto* editor = new QDateEdit(parent);
  editor->setCalendarPopup(true);
  editor->setDisplayFormat(QStringLiteral("MMM d, yyyy"));
  return editor;
}

void ProjectDateEditDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
  auto* date_edit = qobject_cast<QDateEdit*>(editor);
  if (date_edit == nullptr) {
    return;
  }
  const QDate date = index.data(Qt::EditRole).toDate();
  date_edit->setDate(date.isValid() ? date : QDate::currentDate());
}

void ProjectDateEditDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                           const QModelIndex& index) const {
  auto* date_edit = qobject_cast<QDateEdit*>(editor);
  if (date_edit == nullptr) {
    return;
  }
  model->setData(index, date_edit->date(), Qt::EditRole);
}

void ProjectDateEditDelegate::updateEditorGeometry(QWidget* editor,
                                                   const QStyleOptionViewItem& option,
                                                   const QModelIndex& /*index*/) const {
  editor->setGeometry(option.rect);
}

}  // namespace cppwiki::gui::project_board
