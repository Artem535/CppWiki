#ifndef CPPWIKI_SRC_GUI_DOCUMENT_TREE_VIEW_H_
#define CPPWIKI_SRC_GUI_DOCUMENT_TREE_VIEW_H_

#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QRect>
#include <QTreeView>

class QAbstractItemModel;
class QEvent;
class QMouseEvent;
class QPainter;
class QStyleOptionViewItem;

namespace cppwiki::gui {

class DocumentTreeView final : public QTreeView {
  Q_OBJECT

 public:
  explicit DocumentTreeView(QWidget* parent = nullptr);
  void setModel(QAbstractItemModel* model) override;

  signals:
   void addChildRequested(const QModelIndex& parent_index);

protected:
  void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override;
  void drawRow(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;

private:
  [[nodiscard]] QRect addChildButtonRect(const QModelIndex& index) const;
  [[nodiscard]] bool canShowAddChildButton(const QModelIndex& index) const;
  void resetPressedAddChildIndex();
  void setHoveredIndex(const QModelIndex& index);
  void updateRow(const QModelIndex& index) const;

  QModelIndex hovered_index_;
  QPersistentModelIndex pressed_add_child_index_;

  static constexpr int kAddChildButtonSize = 24;
  static constexpr int kRowHorizontalMargin = 8;
  static constexpr int kRowVerticalPadding = 8;
};

}  // namespace cppwiki::gui

#endif  // CPPWIKI_SRC_GUI_DOCUMENT_TREE_VIEW_H_
