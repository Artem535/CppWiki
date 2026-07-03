#include "gui/merge/conflict_merge_dialog.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QQuickWidget>
#include <QQmlContext>
#include <QStatusBar>
#include <QUrl>
#include <QVBoxLayout>

#include <optional>
#include <utility>

#include "core/qt_string.h"
#include "gui/merge/conflict_merge_model.h"
#include "gui/merge/conflict_merge_resolution.h"
#include "gui/page_helpers.h"

namespace cppwiki::gui::merge {

namespace {

auto ParseCurrentDocument(storage::LocalDocumentRepository& repository,
                          std::string_view document_id) -> std::optional<storage::DocumentRecord> {
  const auto loaded = repository.LoadDocument(document_id);
  if (loaded.error || !loaded.document.has_value()) {
    return std::nullopt;
  }
  return loaded.document;
}

}  // namespace

ConflictMergeDialog::ConflictMergeDialog(
    std::shared_ptr<storage::LocalDocumentRepository> repository, QString conflict_id,
    QString author_id, QWidget* parent)
    : QDialog(parent),
      repository_(std::move(repository)),
      conflict_id_(std::move(conflict_id)),
      author_id_(std::move(author_id)) {
  setWindowTitle(QStringLiteral("Merge conflict"));
  resize(980, 720);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(8);

  auto* header = new QLabel(QStringLiteral("Loading conflict..."), this);
  header->setObjectName(QStringLiteral("mergeEditorHeader"));
  header->setWordWrap(true);
  layout->addWidget(header);

  model_ = new ConflictMergeModel(this);
  view_ = new QQuickWidget(this);
  view_->setResizeMode(QQuickWidget::SizeRootObjectToView);
  view_->rootContext()->setContextProperty(QStringLiteral("mergeModel"), model_);
  view_->rootContext()->setContextProperty(QStringLiteral("mergeDialog"), this);
  view_->setSource(QUrl(QStringLiteral("qrc:/cppwiki/gui/qml/ConflictMergeEditor.qml")));
  layout->addWidget(view_, 1);

  auto* buttons = new QDialogButtonBox(this);
  auto* cancel_button = buttons->addButton(QStringLiteral("Cancel"), QDialogButtonBox::RejectRole);
  auto* save_button = buttons->addButton(QStringLiteral("Save merge"), QDialogButtonBox::AcceptRole);
  connect(cancel_button, &QAbstractButton::clicked, this, &ConflictMergeDialog::cancelMerge);
  connect(save_button, &QAbstractButton::clicked, this, &ConflictMergeDialog::saveMerge);
  layout->addWidget(buttons);

  Initialize();
}

ConflictMergeDialog::~ConflictMergeDialog() = default;

auto ConflictMergeDialog::statusMessage() const -> QString { return status_message_; }

auto ConflictMergeDialog::busy() const -> bool { return busy_; }

auto ConflictMergeDialog::conflictTitle() const -> QString {
  return model_ == nullptr ? QStringLiteral("Merge conflict") : model_->ConflictTitle();
}

void ConflictMergeDialog::setResolution(int row, const QString& resolution) {
  if (model_ == nullptr) {
    return;
  }
  model_->SetResolution(row, resolution);
}

void ConflictMergeDialog::useAllLocal() {
  if (model_ == nullptr) {
    return;
  }
  model_->SetResolutionForAll(QStringLiteral("local"));
  SetStatusMessage(QStringLiteral("Using local version for all blocks."));
}

void ConflictMergeDialog::useAllRemote() {
  if (model_ == nullptr) {
    return;
  }
  model_->SetResolutionForAll(QStringLiteral("remote"));
  SetStatusMessage(QStringLiteral("Using remote version for all blocks."));
}

void ConflictMergeDialog::saveMerge() {
  if (repository_ == nullptr || model_ == nullptr || busy_) {
    return;
  }

  auto merged = model_->BuildMergedDocument();
  if (!merged.has_value()) {
    SetStatusMessage(QStringLiteral("Cannot save: merged document is empty."));
    return;
  }

  SetBusy(true);
  const auto result =
      ApplyMergedConflictResolution(*repository_, conflict_id_, author_id_, *merged);
  SetBusy(false);
  SetStatusMessage(result.message);
  if (result.status != MergeResolutionStatus::kApplied) {
    return;
  }

  emit mergeApplied();
  accept();
}

void ConflictMergeDialog::cancelMerge() { reject(); }

void ConflictMergeDialog::SetStatusMessage(QString message) {
  if (status_message_ == message) {
    return;
  }
  status_message_ = std::move(message);
  emit statusMessageChanged();
}

void ConflictMergeDialog::SetBusy(bool busy) {
  if (busy_ == busy) {
    return;
  }
  busy_ = busy;
  emit busyChanged();
}

void ConflictMergeDialog::Initialize() {
  if (repository_ == nullptr || model_ == nullptr) {
    SetStatusMessage(QStringLiteral("Merge editor is unavailable."));
    return;
  }

  const auto conflict = repository_->LoadConflict(conflict_id_.toStdString());
  if (conflict.error || !conflict.conflict.has_value()) {
    SetStatusMessage(QStringLiteral("Conflict could not be loaded."));
    return;
  }

  const auto current_document = ParseCurrentDocument(*repository_, conflict.conflict->document_id);
  if (!model_->LoadConflict(*conflict.conflict, current_document)) {
    SetStatusMessage(model_->ErrorMessage());
    return;
  }

  SetStatusMessage(QStringLiteral("Choose a resolution for each block, then save."));
}

}  // namespace cppwiki::gui::merge
