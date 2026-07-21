#include "gui/import_destination_dialog.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeView>
#include <QVBoxLayout>

#include "gui/document_tree_model.h"

namespace cppwiki::gui {

ImportDestinationDialog::ImportDestinationDialog(DocumentTreeModel* model,
                                                 const QString& suggested_title, QWidget* parent)
    : QDialog(parent), model_(model) {
  setWindowTitle(QStringLiteral("Import document"));
  resize(420, 420);

  auto* layout = new QVBoxLayout(this);

  layout->addWidget(new QLabel(QStringLiteral("Save into:"), this));

  tree_view_ = new QTreeView(this);
  tree_view_->setModel(model_);
  tree_view_->setHeaderHidden(true);
  tree_view_->setDragEnabled(false);
  tree_view_->setAcceptDrops(false);
  tree_view_->setSelectionMode(QAbstractItemView::SingleSelection);
  tree_view_->expandAll();
  layout->addWidget(tree_view_, 1);

  layout->addWidget(new QLabel(QStringLiteral("Title:"), this));
  title_edit_ = new QLineEdit(suggested_title, this);
  layout->addWidget(title_edit_);

  button_box_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(button_box_, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(button_box_, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(button_box_);

  connect(tree_view_->selectionModel(), &QItemSelectionModel::currentChanged, this,
          [this]() { UpdateOkEnabled(); });
  connect(title_edit_, &QLineEdit::textChanged, this, [this]() { UpdateOkEnabled(); });
  connect(this, &QDialog::accepted, this, [this]() {
    const auto index = tree_view_->currentIndex();
    chosen_workspace_id_ = QString::fromStdString(model_->workspaceId(index).value_or("default"));
    chosen_parent_document_id_ = model_->documentId(index);
  });

  UpdateOkEnabled();
}

QString ImportDestinationDialog::chosenTitle() const {
  return title_edit_->text().trimmed();
}

void ImportDestinationDialog::UpdateOkEnabled() {
  const auto index = tree_view_->currentIndex();
  const bool has_valid_target =
      index.isValid() && !index.data(DocumentTreeModel::kIsActionRole).toBool();
  const bool has_title = !title_edit_->text().trimmed().isEmpty();
  button_box_->button(QDialogButtonBox::Ok)->setEnabled(has_valid_target && has_title);
}

}  // namespace cppwiki::gui
