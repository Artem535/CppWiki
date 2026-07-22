#include "gui/project_board/kanban/kanban_task_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace cppwiki::gui::kanban {

namespace {

// Tags/assignees are edited as a single comma-separated line rather than a dedicated multi-value
// widget -- this is a small internal form, not a general-purpose tag picker, and a plain QLineEdit
// round-trips cleanly through KanbanTask::tags/users (QStringList) without extra chrome.
auto SplitCommaList(const QString& text) -> QStringList {
  QStringList result;
  for (const QString& part : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
    const QString trimmed = part.trimmed();
    if (!trimmed.isEmpty()) {
      result.append(trimmed);
    }
  }
  return result;
}

}  // namespace

KanbanTaskDialog::KanbanTaskDialog(QWidget* parent, const QVector<KanbanColumn>& columns)
    : QDialog(parent) {
  text_edit_ = new QLineEdit(this);

  epic_check_ = new QCheckBox(QStringLiteral("Epic (its own swimlane, no status)"), this);

  column_combo_ = new QComboBox(this);
  for (const KanbanColumn& column : columns) {
    column_combo_->addItem(column.label, column.id);
  }
  // An epic is a swimlane header, not a per-column card (see KanbanBoardModel::rows()), so its
  // status is meaningless -- disable rather than hide, so toggling "Epic" back off returns the
  // column selection the user already made instead of losing it.
  connect(epic_check_, &QCheckBox::toggled, column_combo_, &QComboBox::setDisabled);

  priority_combo_ = new QComboBox(this);
  priority_combo_->addItem(QStringLiteral("None"), 0);
  priority_combo_->addItem(PriorityLabel(kPriorityLow), kPriorityLow);
  priority_combo_->addItem(PriorityLabel(kPriorityMedium), kPriorityMedium);
  priority_combo_->addItem(PriorityLabel(kPriorityHigh), kPriorityHigh);

  progress_spin_ = new QSpinBox(this);
  progress_spin_->setRange(0, 100);
  progress_spin_->setSuffix(QStringLiteral("%"));

  tags_edit_ = new QLineEdit(this);
  tags_edit_->setPlaceholderText(QStringLiteral("comma-separated, e.g. design, backend"));

  users_edit_ = new QLineEdit(this);
  users_edit_->setPlaceholderText(QStringLiteral("comma-separated names"));

  description_edit_ = new QPlainTextEdit(this);
  description_edit_->setFixedHeight(80);

  auto* form_layout = new QFormLayout();
  form_layout->addRow(QStringLiteral("Task"), text_edit_);
  form_layout->addRow(QString(), epic_check_);
  form_layout->addRow(QStringLiteral("Status"), column_combo_);
  form_layout->addRow(QStringLiteral("Priority"), priority_combo_);
  form_layout->addRow(QStringLiteral("Progress"), progress_spin_);
  form_layout->addRow(QStringLiteral("Tags"), tags_edit_);
  form_layout->addRow(QStringLiteral("Assignees"), users_edit_);
  form_layout->addRow(QStringLiteral("Description"), description_edit_);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto* layout = new QVBoxLayout(this);
  layout->addLayout(form_layout);
  layout->addWidget(buttons);
}

void KanbanTaskDialog::SetInitialValues(const KanbanTask& task) {
  text_edit_->setText(task.text);
  epic_check_->setChecked(task.IsEpic());
  const int column_position = column_combo_->findData(task.column);
  column_combo_->setCurrentIndex(column_position >= 0 ? column_position : 0);
  const int priority_position = priority_combo_->findData(task.priority);
  priority_combo_->setCurrentIndex(priority_position >= 0 ? priority_position : 0);
  progress_spin_->setValue(static_cast<int>(task.progress));
  tags_edit_->setText(task.tags.join(QStringLiteral(", ")));
  users_edit_->setText(task.users.join(QStringLiteral(", ")));
  description_edit_->setPlainText(task.description);
}

auto KanbanTaskDialog::ToResult() const -> Result {
  Result result;
  result.text = text_edit_->text().trimmed();
  result.is_epic = epic_check_->isChecked();
  result.column_id = column_combo_->currentData().toString();
  result.priority = priority_combo_->currentData().toInt();
  result.progress = progress_spin_->value();
  result.tags = SplitCommaList(tags_edit_->text());
  result.users = SplitCommaList(users_edit_->text());
  result.description = description_edit_->toPlainText().trimmed();
  return result;
}

auto KanbanTaskDialog::RequestNewTask(QWidget* parent, const QVector<KanbanColumn>& columns,
                                      bool default_is_epic) -> std::optional<Result> {
  KanbanTaskDialog dialog(parent, columns);
  dialog.setWindowTitle(default_is_epic ? QStringLiteral("New epic") : QStringLiteral("New task"));
  dialog.epic_check_->setChecked(default_is_epic);
  if (dialog.exec() != QDialog::Accepted) {
    return std::nullopt;
  }
  const Result result = dialog.ToResult();
  if (result.text.isEmpty()) {
    return std::nullopt;
  }
  return result;
}

auto KanbanTaskDialog::RequestEditTask(QWidget* parent, const QVector<KanbanColumn>& columns,
                                       const KanbanTask& task) -> std::optional<Result> {
  KanbanTaskDialog dialog(parent, columns);
  dialog.setWindowTitle(QStringLiteral("Edit task"));
  dialog.SetInitialValues(task);
  if (dialog.exec() != QDialog::Accepted) {
    return std::nullopt;
  }
  const Result result = dialog.ToResult();
  if (result.text.isEmpty()) {
    return std::nullopt;
  }
  return result;
}

}  // namespace cppwiki::gui::kanban
