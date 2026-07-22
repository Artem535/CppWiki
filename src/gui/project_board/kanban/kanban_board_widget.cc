#include "gui/project_board/kanban/kanban_board_widget.h"

#include <QQmlContext>
#include <QUrl>
#include <utility>

namespace cppwiki::gui::kanban {

KanbanBoardWidget::KanbanBoardWidget(QWidget* parent)
    : QQuickWidget(parent), model_(new KanbanBoardModel(this)) {
  setResizeMode(QQuickWidget::SizeRootObjectToView);
  rootContext()->setContextProperty(QStringLiteral("kanbanModel"), model_);
  setSource(QUrl(QStringLiteral("qrc:/cppwiki/kanban/KanbanBoard.qml")));
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

}  // namespace cppwiki::gui::kanban
