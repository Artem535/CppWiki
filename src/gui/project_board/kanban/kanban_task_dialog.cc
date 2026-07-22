#include "gui/project_board/kanban/kanban_task_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace cppwiki::gui::kanban {

KanbanTaskDialog::KanbanTaskDialog(QWidget* parent, const QVector<KanbanColumn>& columns)
    : QDialog(parent) {
  text_edit_ = new QLineEdit(this);

  column_combo_ = new QComboBox(this);
  for (const KanbanColumn& column : columns) {
    column_combo_->addItem(column.label, column.id);
  }

  priority_combo_ = new QComboBox(this);
  priority_combo_->addItem(QStringLiteral("None"), 0);
  priority_combo_->addItem(PriorityLabel(kPriorityLow), kPriorityLow);
  priority_combo_->addItem(PriorityLabel(kPriorityMedium), kPriorityMedium);
  priority_combo_->addItem(PriorityLabel(kPriorityHigh), kPriorityHigh);

  progress_spin_ = new QSpinBox(this);
  progress_spin_->setRange(0, 100);
  progress_spin_->setSuffix(QStringLiteral("%"));

  auto* form_layout = new QFormLayout();
  form_layout->addRow(QStringLiteral("Task"), text_edit_);
  form_layout->addRow(QStringLiteral("Status"), column_combo_);
  form_layout->addRow(QStringLiteral("Priority"), priority_combo_);
  form_layout->addRow(QStringLiteral("Progress"), progress_spin_);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto* layout = new QVBoxLayout(this);
  layout->addLayout(form_layout);
  layout->addWidget(buttons);
}

void KanbanTaskDialog::SetInitialValues(const KanbanTask& task) {
  text_edit_->setText(task.text);
  const int column_position = column_combo_->findData(task.column);
  column_combo_->setCurrentIndex(column_position >= 0 ? column_position : 0);
  const int priority_position = priority_combo_->findData(task.priority);
  priority_combo_->setCurrentIndex(priority_position >= 0 ? priority_position : 0);
  progress_spin_->setValue(static_cast<int>(task.progress));
}

auto KanbanTaskDialog::ToResult() const -> Result {
  Result result;
  result.text = text_edit_->text().trimmed();
  result.column_id = column_combo_->currentData().toString();
  result.priority = priority_combo_->currentData().toInt();
  result.progress = progress_spin_->value();
  return result;
}

auto KanbanTaskDialog::RequestNewTask(QWidget* parent, const QVector<KanbanColumn>& columns)
    -> std::optional<Result> {
  KanbanTaskDialog dialog(parent, columns);
  dialog.setWindowTitle(QStringLiteral("New task"));
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
