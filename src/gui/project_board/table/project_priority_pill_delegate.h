#ifndef CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_PRIORITY_PILL_DELEGATE_H_
#define CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_PRIORITY_PILL_DELEGATE_H_

#include <QStyledItemDelegate>

class QComboBox;

namespace cppwiki::gui::project_board {

// Renders the Priority column as a small colored rounded pill (see PaintPill) instead of plain
// text, and edits it via a QComboBox over the fixed Low/Medium/High levels (plus a "None" entry to
// clear it) — mirroring GridTab's PriorityCell + its richselect editor in the web version.
class ProjectPriorityPillDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  explicit ProjectPriorityPillDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option,
                               const QModelIndex& index) const override;

 protected:
  // See ProjectStatusPillDelegate::initStyleOption() for why this override (not clearing
  // option.text on a local copy passed into QStyledItemDelegate::paint()) is the fix.
  void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override;

 public:
  [[nodiscard]] QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel* model,
                    const QModelIndex& index) const override;
  void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const override;
};

}  // namespace cppwiki::gui::project_board

#endif  // CPPWIKI_SRC_GUI_PROJECT_BOARD_TABLE_PROJECT_PRIORITY_PILL_DELEGATE_H_
