#include "gui/project_board/kanban/kanban_board_widget.h"

#include <QFrame>
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
  // Framed strip (object names styled in cppwiki.qss) instead of two bare QPushButtons floating
  // directly against the tab's edge -- gives the toolbar its own visually-grounded band, matching
  // how the rest of the app (e.g. the collaboration panel's Import/Export controls) frames native
  // action buttons rather than leaving them loose on the surrounding widget's background.
  auto* toolbar = new QFrame(this);
  toolbar->setObjectName(QStringLiteral("kanbanToolbar"));
  toolbar->setAttribute(Qt::WA_StyledBackground, true);

  auto* add_column_button = new QPushButton(QStringLiteral("Add column"), toolbar);
  auto* add_task_button = new QPushButton(QStringLiteral("Add task"), toolbar);
  auto* add_epic_button = new QPushButton(QStringLiteral("Add epic"), toolbar);
  add_column_button->setObjectName(QStringLiteral("kanbanToolbarButton"));
  add_task_button->setObjectName(QStringLiteral("kanbanToolbarButton"));
  add_epic_button->setObjectName(QStringLiteral("kanbanToolbarButton"));
  add_column_button->setCursor(Qt::PointingHandCursor);
  add_task_button->setCursor(Qt::PointingHandCursor);
  add_epic_button->setCursor(Qt::PointingHandCursor);
  connect(add_column_button, &QPushButton::clicked, this,
          &KanbanBoardWidget::HandleAddColumnClicked);
  connect(add_task_button, &QPushButton::clicked, this, &KanbanBoardWidget::HandleAddTaskClicked);
  connect(add_epic_button, &QPushButton::clicked, this, &KanbanBoardWidget::HandleAddEpicClicked);

  auto* toolbar_layout = new QHBoxLayout(toolbar);
  toolbar_layout->setContentsMargins(12, 8, 12, 8);
  toolbar_layout->setSpacing(8);
  toolbar_layout->addWidget(add_column_button);
  toolbar_layout->addWidget(add_task_button);
  toolbar_layout->addWidget(add_epic_button);
  toolbar_layout->addStretch(1);

  quick_widget_ = new QQuickWidget(this);
  quick_widget_->setResizeMode(QQuickWidget::SizeRootObjectToView);
  quick_widget_->rootContext()->setContextProperty(QStringLiteral("kanbanModel"), model_);
  quick_widget_->setSource(QUrl(QStringLiteral("qrc:/cppwiki/kanban/KanbanBoard.qml")));

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(toolbar);
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
  model_->addTask(result->text, result->column_id, result->priority, result->progress,
                  result->is_epic, result->description, result->tags, result->users,
                  result->start, result->duration);
}

void KanbanBoardWidget::HandleAddEpicClicked() {
  const auto result = KanbanTaskDialog::RequestNewTask(this, model_->ExportDocument().columns,
                                                       /*default_is_epic=*/true);
  if (!result.has_value()) {
    return;
  }
  model_->addTask(result->text, result->column_id, result->priority, result->progress,
                  result->is_epic, result->description, result->tags, result->users,
                  result->start, result->duration);
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
  model_->updateTask(task_id, result->text, result->column_id, result->priority, result->progress,
                     result->is_epic, result->description, result->tags, result->users,
                     result->start, result->duration);
}

}  // namespace cppwiki::gui::kanban
