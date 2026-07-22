#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_DATE_EDIT_DELEGATE_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_DATE_EDIT_DELEGATE_H_

#include <QStyledItemDelegate>

namespace cppwiki::gui::project_board {

// Edits the Start column via a real QDateEdit with a calendar popup (not a plain text field) —
// mirroring GridTab's "datepicker" editor. Display formatting ("Jul 24, 2026") is handled by
// ProjectTaskTableModel's Qt::DisplayRole itself, so this delegate only needs to supply the
// editor.
class ProjectDateEditDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  explicit ProjectDateEditDelegate(QObject* parent = nullptr);

  [[nodiscard]] QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel* model,
                    const QModelIndex& index) const override;
  void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const override;
};

}  // namespace cppwiki::gui::project_board

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_DATE_EDIT_DELEGATE_H_
