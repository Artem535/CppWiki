#ifndef CPPWIKI_SRC_GUI_DOCUMENT_TREE_VIEW_H_
#define CPPWIKI_SRC_GUI_DOCUMENT_TREE_VIEW_H_

#include <QModelIndex>
#include <QTreeView>

namespace cppwiki::gui {

class DocumentTreeView final : public QTreeView {
  Q_OBJECT

 public:
  explicit DocumentTreeView(QWidget* parent = nullptr);

 signals:
  void addChildRequested(const QModelIndex& parent_index);

 protected:
  void drawBranches(QPainter* painter, const QRect& rect,
                    const QModelIndex& index) const override;
  void drawRow(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;

 private:
  [[nodiscard]] QRect addChildButtonRect(const QModelIndex& index) const;
  [[nodiscard]] bool canShowAddChildButton(const QModelIndex& index) const;

  QModelIndex hovered_index_;
  QModelIndex pressed_add_child_index_;
};

}  // namespace cppwiki::gui

#endif  // CPPWIKI_SRC_GUI_DOCUMENT_TREE_VIEW_H_
