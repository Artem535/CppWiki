#ifndef CPPWIKI_SRC_GUI_IMPORT_DESTINATION_DIALOG_H_
#define CPPWIKI_SRC_GUI_IMPORT_DESTINATION_DIALOG_H_

#include <QDialog>
#include <QModelIndex>
#include <optional>
#include <string>

class QLineEdit;
class QTreeView;
class QDialogButtonBox;

namespace cppwiki::gui {

class DocumentTreeModel;

// Lets the user pick where an imported file should be saved as a new document (issue #102
// follow-up): a location in the existing document tree (a workspace root, or an existing
// document to import as a child of) plus a title for the new document. Reuses the same
// DocumentTreeModel already populating the sidebar tree — read-only here, no drag/drop, no
// "+ add child" affordance is relevant, so a plain QTreeView is bound to it rather than the
// sidebar's DocumentTreeView.
class ImportDestinationDialog final : public QDialog {
  Q_OBJECT

 public:
  // `model` is not owned; it must outlive the dialog (the caller's existing workspace tree
  // model, shared with the sidebar). `suggested_title` pre-fills the name field, e.g. derived
  // from the imported file's name.
  explicit ImportDestinationDialog(DocumentTreeModel* model, const QString& suggested_title,
                                   QWidget* parent = nullptr);

  // Valid only after the dialog was accepted. Returns the chosen workspace id and, if the
  // selected destination was an existing document rather than a workspace root, that
  // document's id as the new document's parent.
  [[nodiscard]] QString chosenWorkspaceId() const {
    return chosen_workspace_id_;
  }
  [[nodiscard]] std::optional<std::string> chosenParentDocumentId() const {
    return chosen_parent_document_id_;
  }
  [[nodiscard]] QString chosenTitle() const;

 private:
  void UpdateOkEnabled();

  DocumentTreeModel* model_ = nullptr;
  QTreeView* tree_view_ = nullptr;
  QLineEdit* title_edit_ = nullptr;
  QDialogButtonBox* button_box_ = nullptr;
  QString chosen_workspace_id_;
  std::optional<std::string> chosen_parent_document_id_;
};

}  // namespace cppwiki::gui

#endif  // CPPWIKI_SRC_GUI_IMPORT_DESTINATION_DIALOG_H_
