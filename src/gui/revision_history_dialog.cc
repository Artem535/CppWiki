#include "gui/revision_history_dialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <utility>

#include "bridge/editor_bridge.h"

namespace cppwiki::gui {

RevisionHistoryDialog::RevisionHistoryDialog(bridge::QEditorBridge* bridge, QString page_id,
                                             QWidget* parent)
    : QDialog(parent), bridge_(bridge), page_id_(std::move(page_id)) {
  setWindowTitle(QStringLiteral("Version history"));
  resize(420, 480);

  auto* outer_layout = new QVBoxLayout(this);

  auto* scroll_area = new QScrollArea(this);
  scroll_area->setWidgetResizable(true);
  scroll_area->setFrameShape(QFrame::NoFrame);

  rows_container_ = new QWidget(scroll_area);
  rows_layout_ = new QVBoxLayout(rows_container_);
  rows_layout_->setContentsMargins(0, 0, 0, 0);
  rows_layout_->addStretch(1);
  scroll_area->setWidget(rows_container_);
  outer_layout->addWidget(scroll_area, 1);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  outer_layout->addWidget(buttons);

  RefreshList();
}

void RevisionHistoryDialog::RefreshList() {
  while (auto* item = rows_layout_->takeAt(0)) {
    if (auto* widget = item->widget()) widget->deleteLater();
    delete item;
  }

  if (!bridge_) {
    rows_layout_->addStretch(1);
    return;
  }

  const auto response = bridge_->listDocumentRevisions(page_id_);
  if (!response.value(QStringLiteral("ok")).toBool()) {
    const auto error = response.value(QStringLiteral("error")).toMap();
    auto* error_label = new QLabel(error.value(QStringLiteral("message")).toString(), rows_container_);
    error_label->setWordWrap(true);
    rows_layout_->addWidget(error_label);
    rows_layout_->addStretch(1);
    return;
  }

  const auto revisions = response.value(QStringLiteral("result")).toList();
  if (revisions.isEmpty()) {
    auto* empty_label = new QLabel(QStringLiteral("No prior revisions yet."), rows_container_);
    empty_label->setWordWrap(true);
    rows_layout_->addWidget(empty_label);
    rows_layout_->addStretch(1);
    return;
  }

  for (const auto& entry : revisions) {
    const auto revision = entry.toMap();
    const auto revision_id = revision.value(QStringLiteral("id")).toString();
    const auto title = revision.value(QStringLiteral("title")).toString();
    const auto saved_at = revision.value(QStringLiteral("savedAt")).toString();

    auto* row = new QWidget(rows_container_);
    auto* row_layout = new QHBoxLayout(row);
    row_layout->setContentsMargins(4, 4, 4, 4);

    auto* label = new QLabel(QStringLiteral("%1\n%2").arg(title, saved_at), row);
    label->setWordWrap(true);
    row_layout->addWidget(label, 1);

    auto* restore_button = new QPushButton(QStringLiteral("Restore"), row);
    connect(restore_button, &QPushButton::clicked, this,
            [this, revision_id]() { RestoreRevision(revision_id); });
    row_layout->addWidget(restore_button);

    rows_layout_->addWidget(row);
  }
  rows_layout_->addStretch(1);
}

void RevisionHistoryDialog::RestoreRevision(const QString& revision_id) {
  if (!bridge_) return;

  const auto response = bridge_->restoreDocumentRevision(page_id_, revision_id);
  if (!response.value(QStringLiteral("ok")).toBool()) {
    const auto error = response.value(QStringLiteral("error")).toMap();
    QMessageBox::warning(this, QStringLiteral("Restore failed"),
                         error.value(QStringLiteral("message")).toString());
    return;
  }

  RefreshList();
}

}  // namespace cppwiki::gui
