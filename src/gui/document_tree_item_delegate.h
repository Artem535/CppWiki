#ifndef CPPWIKI_SRC_GUI_DOCUMENT_TREE_ITEM_DELEGATE_H_
#define CPPWIKI_SRC_GUI_DOCUMENT_TREE_ITEM_DELEGATE_H_

#include <QModelIndex>
#include <QPainter>
#include <QSize>
#include <QStyledItemDelegate>

// Forward declaration for Qlementine style
namespace oclero::qlementine {
class QlementineStyle;
}

namespace cppwiki::gui {

// Custom delegate for document tree items with Qlementine styling.
// The delegate paints the row content and the hover-only "+" affordance.
// Click handling for the "+" button lives in DocumentTreeView.
class DocumentTreeItemDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  explicit DocumentTreeItemDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;

  [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option,
                               const QModelIndex& index) const override;

 protected:
  void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override;

 private:
  [[nodiscard]] oclero::qlementine::QlementineStyle* getQlementineStyle() const;
  [[nodiscard]] QRect addChildButtonRect(const QStyleOptionViewItem& option,
                                         const QModelIndex& index) const;

  static constexpr int kIconSize = 20;
  static constexpr int kAddChildButtonSize = 24;
};

}  // namespace cppwiki::gui

#endif  // CPPWIKI_SRC_GUI_DOCUMENT_TREE_ITEM_DELEGATE_H_
