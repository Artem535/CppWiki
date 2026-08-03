#include "gui/trash_dialog.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QVariantList>
#include <QVariantMap>

#include "bridge/editor_bridge.h"

namespace cppwiki::gui {

namespace {

auto ClearLayout(QVBoxLayout* layout) -> void {
  while (auto* item = layout->takeAt(0)) {
    if (auto* widget = item->widget()) {
      widget->deleteLater();
    }
    delete item;
  }
}

}  // namespace

TrashDialog::TrashDialog(bridge::QEditorBridge* bridge, QWidget* parent)
    : QDialog(parent), bridge_(bridge) {
  setWindowTitle(QStringLiteral("Trash"));
  setMinimumSize(420, 320);

  auto* layout = new QVBoxLayout(this);

  auto* scroll_area = new QScrollArea(this);
  scroll_area->setWidgetResizable(true);
  scroll_area->setFrameShape(QFrame::NoFrame);
  rows_container_ = new QWidget(scroll_area);
  rows_layout_ = new QVBoxLayout(rows_container_);
  rows_layout_->setContentsMargins(0, 0, 0, 0);
  rows_layout_->addStretch(1);
  scroll_area->setWidget(rows_container_);
  layout->addWidget(scroll_area, 1);

  auto* button_row = new QHBoxLayout();
  auto* empty_trash_button = new QPushButton(QStringLiteral("Empty Trash"), this);
  empty_trash_button->setObjectName(QStringLiteral("emptyTrashButton"));
  connect(empty_trash_button, &QPushButton::clicked, this, &TrashDialog::EmptyTrash);
  button_row->addWidget(empty_trash_button);
  button_row->addStretch(1);
  layout->addLayout(button_row);

  auto* button_box = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  layout->addWidget(button_box);

  RefreshList();
}

void TrashDialog::RefreshList() {
  ClearLayout(rows_layout_);

  if (!bridge_) {
    return;
  }

  const auto response = bridge_->listTrash();
  const auto pages = response.value(QStringLiteral("result")).toList();

  if (pages.isEmpty()) {
    auto* empty_label = new QLabel(QStringLiteral("Trash is empty."), rows_container_);
    empty_label->setAlignment(Qt::AlignCenter);
    rows_layout_->insertWidget(0, empty_label);
    return;
  }

  for (const auto& page_variant : pages) {
    const auto page = page_variant.toMap();
    const auto page_id = page.value(QStringLiteral("id")).toString();
    const auto title = page.value(QStringLiteral("title")).toString();
    const auto trashed_at = page.value(QStringLiteral("trashedAt")).toString();

    auto* row = new QFrame(rows_container_);
    row->setObjectName(QStringLiteral("trashRow"));
    auto* row_layout = new QHBoxLayout(row);

    auto* text_column = new QVBoxLayout();
    auto* title_label =
        new QLabel(title.isEmpty() ? QStringLiteral("(untitled)") : title, row);
    auto* trashed_at_label =
        new QLabel(QStringLiteral("Deleted: %1").arg(trashed_at), row);
    trashed_at_label->setEnabled(false);
    text_column->addWidget(title_label);
    text_column->addWidget(trashed_at_label);
    row_layout->addLayout(text_column, 1);

    auto* restore_button = new QPushButton(QStringLiteral("Restore"), row);
    connect(restore_button, &QPushButton::clicked, this,
            [this, page_id]() { RestoreDocument(page_id); });
    row_layout->addWidget(restore_button);

    auto* delete_button = new QPushButton(QStringLiteral("Delete Permanently"), row);
    connect(delete_button, &QPushButton::clicked, this,
            [this, page_id]() { PermanentlyDeleteDocument(page_id); });
    row_layout->addWidget(delete_button);

    rows_layout_->insertWidget(rows_layout_->count() - 1, row);
  }
}

void TrashDialog::RestoreDocument(const QString& page_id) {
  if (!bridge_) {
    return;
  }
  bridge_->restoreDocument(page_id);
  RefreshList();
}

void TrashDialog::PermanentlyDeleteDocument(const QString& page_id) {
  if (!bridge_) {
    return;
  }
  const auto confirmed =
      QMessageBox::question(this, QStringLiteral("Delete Permanently"),
                            QStringLiteral("Permanently delete this page and its subpages? "
                                          "This cannot be undone."),
                            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
  if (confirmed != QMessageBox::Yes) {
    return;
  }
  bridge_->permanentlyDeleteDocument(page_id);
  RefreshList();
}

void TrashDialog::EmptyTrash() {
  if (!bridge_) {
    return;
  }
  const auto confirmed = QMessageBox::question(
      this, QStringLiteral("Empty Trash"),
      QStringLiteral("Permanently delete everything in the trash? This cannot be undone."),
      QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
  if (confirmed != QMessageBox::Yes) {
    return;
  }
  bridge_->emptyTrash();
  RefreshList();
}

}  // namespace cppwiki::gui
