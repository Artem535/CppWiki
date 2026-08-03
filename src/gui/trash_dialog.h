#ifndef CPPWIKI_SRC_GUI_TRASH_DIALOG_H_
#define CPPWIKI_SRC_GUI_TRASH_DIALOG_H_

#include <QDialog>
#include <QPointer>
#include <QString>

class QVBoxLayout;
class QWidget;

namespace cppwiki::bridge {
class QEditorBridge;
}

namespace cppwiki::gui {

// Issue #165: a minimal, native view onto the current workspace's trash -- lists pages
// soft-deleted via EditorBridge::deleteDocument(), each restorable or permanently deletable, plus
// a bulk "Empty Trash" action. All operations go through EditorBridge (which owns the
// trash/restore/permanent-delete tree-walking logic); this dialog only renders the result and
// re-lists after each action.
class TrashDialog final : public QDialog {
  Q_OBJECT

 public:
  // `bridge` must outlive this dialog -- it is owned by the Page that constructs the dialog and
  // is only used for the dialog's modal lifetime, matching how Page uses editor_bridge_ itself.
  explicit TrashDialog(bridge::QEditorBridge* bridge, QWidget* parent = nullptr);

 private:
  void RefreshList();
  void RestoreDocument(const QString& page_id);
  void PermanentlyDeleteDocument(const QString& page_id);
  void EmptyTrash();

  QPointer<bridge::QEditorBridge> bridge_;
  QVBoxLayout* rows_layout_ = nullptr;
  QWidget* rows_container_ = nullptr;
};

}  // namespace cppwiki::gui

#endif  // CPPWIKI_SRC_GUI_TRASH_DIALOG_H_
