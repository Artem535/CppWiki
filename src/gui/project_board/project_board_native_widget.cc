#include "gui/project_board/project_board_native_widget.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTabWidget>
#include <QVBoxLayout>

#include "gui/project_board/gantt/project_board_gantt_widget.h"
#include "gui/project_board/kanban/kanban_board_model.h"
#include "gui/project_board/kanban/kanban_board_widget.h"
#include "gui/project_board/table/project_task.h"
#include "gui/project_board/table/project_task_table_widget.h"

namespace cppwiki::gui::project_board {

namespace {

QByteArray ToCompactJson(const QJsonObject& object) {
  return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

}  // namespace

ProjectBoardNativeWidget::ProjectBoardNativeWidget(QWidget* parent) : QWidget(parent) {
  gantt_widget_ = new gantt::ProjectBoardGanttWidget(this);
  kanban_widget_ = new kanban::KanbanBoardWidget(this);
  table_widget_ = new ProjectTaskTableWidget(this);

  tabs_ = new QTabWidget(this);
  tabs_->addTab(gantt_widget_, QStringLiteral("Gantt"));
  tabs_->addTab(kanban_widget_, QStringLiteral("Kanban"));
  tabs_->addTab(table_widget_, QStringLiteral("Table"));

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(tabs_);

  connect(gantt_widget_, &gantt::ProjectBoardGanttWidget::DataChanged, this,
          [this](const QJsonObject&) { HandleGanttEdited(); });
  connect(kanban_widget_->Model(), &kanban::KanbanBoardModel::boardChanged, this,
          [this]() { HandleKanbanEdited(); });
  connect(table_widget_, &ProjectTaskTableWidget::documentEdited, this,
          [this]() { HandleTableEdited(); });
}

void ProjectBoardNativeWidget::LoadFromJson(const QByteArray& json) {
  loading_ = true;

  const auto board = QJsonDocument::fromJson(json).object();
  links_ = board.value(QStringLiteral("links")).toArray();

  gantt_widget_->LoadFromJson(board);
  kanban_widget_->LoadFromJson(json);
  const auto table_document = ParseProjectBoardJson(QString::fromUtf8(json));
  if (table_document.has_value()) {
    table_widget_->setDocument(*table_document);
  }

  loading_ = false;
}

QByteArray ProjectBoardNativeWidget::ToJson() const {
  // Every *Edited() handler below reloads Gantt from whichever tab actually changed, so Gantt
  // always holds the authoritative, up-to-date document regardless of which tab was last edited
  // -- see the class doc comment for why Gantt specifically is the one tab that round-trips
  // `links` correctly on its own.
  return ToCompactJson(gantt_widget_->ToJson());
}

void ProjectBoardNativeWidget::HandleGanttEdited() {
  if (loading_) {
    return;
  }
  const auto board = gantt_widget_->ToJson();
  links_ = board.value(QStringLiteral("links")).toArray();
  const auto json = ToCompactJson(board);

  loading_ = true;
  kanban_widget_->LoadFromJson(json);
  const auto table_document = ParseProjectBoardJson(QString::fromUtf8(json));
  if (table_document.has_value()) {
    table_widget_->setDocument(*table_document);
  }
  loading_ = false;

  emit documentEdited();
}

void ProjectBoardNativeWidget::HandleKanbanEdited() {
  if (loading_) {
    return;
  }
  // kanban::KanbanBoardWidget only knows `{tasks, columns}` -- restore the links this widget is
  // tracking on its behalf before broadcasting to the other two tabs, so a Kanban-only edit
  // (moving a card) doesn't silently wipe Gantt's dependency links.
  auto board = QJsonDocument::fromJson(kanban_widget_->ToJson()).object();
  board.insert(QStringLiteral("links"), links_);
  const auto json = ToCompactJson(board);

  loading_ = true;
  gantt_widget_->LoadFromJson(board);
  const auto table_document = ParseProjectBoardJson(QString::fromUtf8(json));
  if (table_document.has_value()) {
    table_widget_->setDocument(*table_document);
  }
  loading_ = false;

  emit documentEdited();
}

void ProjectBoardNativeWidget::HandleTableEdited() {
  if (loading_) {
    return;
  }
  // ProjectTaskTableWidget carries `links` through opaquely from the last setDocument() call, so
  // its own document() already has the right value here -- just keep this widget's own tracked
  // copy in sync too, the same way HandleGanttEdited()/HandleKanbanEdited() do.
  const auto table_document = table_widget_->document();
  links_ = table_document.links;
  const auto json = SerializeProjectBoardJson(table_document).toUtf8();
  const auto board = QJsonDocument::fromJson(json).object();

  loading_ = true;
  gantt_widget_->LoadFromJson(board);
  kanban_widget_->LoadFromJson(json);
  loading_ = false;

  emit documentEdited();
}

}  // namespace cppwiki::gui::project_board
