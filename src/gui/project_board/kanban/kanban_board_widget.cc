#include "gui/project_board/kanban/kanban_board_widget.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QQmlContext>
#include <QQuickWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QVector>
#include <utility>

#include "gui/project_board/kanban/kanban_task_dialog.h"

namespace cppwiki::gui::kanban {

KanbanBoardWidget::KanbanBoardWidget(QWidget* parent)
    : QWidget(parent), model_(new KanbanBoardModel(this)) {
  auto* add_column_button = new QPushButton(QStringLiteral("Add column"), this);
  auto* add_task_button = new QPushButton(QStringLiteral("Add task"), this);
  connect(add_column_button, &QPushButton::clicked, this,
          &KanbanBoardWidget::HandleAddColumnClicked);
  connect(add_task_button, &QPushButton::clicked, this, &KanbanBoardWidget::HandleAddTaskClicked);

  auto* toolbar_layout = new QHBoxLayout();
  toolbar_layout->addWidget(add_column_button);
  toolbar_layout->addWidget(add_task_button);
  toolbar_layout->addStretch(1);

  quick_widget_ = new QQuickWidget(this);
  quick_widget_->setResizeMode(QQuickWidget::SizeRootObjectToView);
  quick_widget_->rootContext()->setContextProperty(QStringLiteral("kanbanModel"), model_);
  quick_widget_->setSource(QUrl(QStringLiteral("qrc:/cppwiki/kanban/KanbanBoard.qml")));

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addLayout(toolbar_layout);
  layout->addWidget(quick_widget_, 1);

  connect(model_, &KanbanBoardModel::editTaskRequested, this,
          &KanbanBoardWidget::HandleEditTaskRequested);
}

void KanbanBoardWidget::LoadFromJson(const QByteArray& json) {
  auto document = KanbanBoardDocument::ParseJson(json);
  if (!document.has_value()) {
    return;
  }
  model_->SetDocument(std::move(*document));
}

auto KanbanBoardWidget::ToJson() const -> QByteArray {
  return model_->ExportDocument().ToJson();
}

auto KanbanBoardWidget::Model() -> KanbanBoardModel* {
  return model_;
}

void KanbanBoardWidget::HandleAddColumnClicked() {
  bool accepted = false;
  const QString label =
      QInputDialog::getText(this, QStringLiteral("New column"), QStringLiteral("Column name"),
                            QLineEdit::Normal, QString(), &accepted);
  if (!accepted || label.trimmed().isEmpty()) {
    return;
  }
  model_->addColumn(label.trimmed());
}

void KanbanBoardWidget::HandleAddTaskClicked() {
  const auto result = KanbanTaskDialog::RequestNewTask(this, model_->ExportDocument().columns);
  if (!result.has_value()) {
    return;
  }
  model_->addTask(result->text, result->column_id, result->priority, result->progress);
}

void KanbanBoardWidget::HandleEditTaskRequested(const QString& task_id) {
  const auto task = model_->FindTask(task_id);
  if (!task.has_value()) {
    return;
  }
  const auto result =
      KanbanTaskDialog::RequestEditTask(this, model_->ExportDocument().columns, *task);
  if (!result.has_value()) {
    return;
  }
  model_->updateTask(task_id, result->text, result->column_id, result->priority, result->progress);
}

}  // namespace cppwiki::gui::kanban
