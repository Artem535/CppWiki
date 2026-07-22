#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_STATUS_PILL_DELEGATE_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_STATUS_PILL_DELEGATE_H_

#include <QStyledItemDelegate>

class QComboBox;

namespace cppwiki::gui::project_board {

// Renders the Status column as a small colored rounded pill (see PaintPill) instead of plain
// text, and edits it via a QComboBox populated from the board's actual status columns —
// mirroring GridTab's StatusCell + its richselect editor in the web version.
class ProjectStatusPillDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  explicit ProjectStatusPillDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option,
                               const QModelIndex& index) const override;

  [[nodiscard]] QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel* model,
                    const QModelIndex& index) const override;
  void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const override;
};

}  // namespace cppwiki::gui::project_board

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_STATUS_PILL_DELEGATE_H_
