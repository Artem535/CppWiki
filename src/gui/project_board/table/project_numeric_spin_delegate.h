#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_NUMERIC_SPIN_DELEGATE_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_NUMERIC_SPIN_DELEGATE_H_

#include <QString>
#include <QStyledItemDelegate>

namespace cppwiki::gui::project_board {

// A configurable QSpinBox-backed editor for a plain integer column — used for both Duration
// (days) and Progress (%) instead of the default free-text line edit, so edits can't produce a
// non-numeric or out-of-range value.
class ProjectNumericSpinDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  ProjectNumericSpinDelegate(int minimum, int maximum, QString suffix, QObject* parent = nullptr);

  [[nodiscard]] QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel* model,
                    const QModelIndex& index) const override;
  void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const override;

 private:
  int minimum_;
  int maximum_;
  QString suffix_;
};

}  // namespace cppwiki::gui::project_board

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_NUMERIC_SPIN_DELEGATE_H_
