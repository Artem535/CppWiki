#include "gui/project_board/gantt/project_board_gantt_widget.h"

#include <KDGanttConstraintModel>
#include <KDGanttView>
#include <QVBoxLayout>

#include "gui/project_board/gantt/project_board_gantt_model.h"

namespace cppwiki::gui::project_board::gantt {

ProjectBoardGanttWidget::ProjectBoardGanttWidget(QWidget* parent)
    : QWidget(parent),
      view_(new KDGantt::View(this)),
      model_(std::make_unique<ProjectBoardGanttModel>()) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(view_);

  view_->setModel(model_.get());
  view_->setConstraintModel(model_->ConstraintModel());

  // Deliberately not connected to modelReset: LoadFromJson() resets the model, and DataChanged
  // is documented (and should be treated by callers) as "the user edited something", not "a
  // document was loaded" — those are different signals for a future integration layer.
  connect(model_.get(), &QAbstractItemModel::dataChanged, this,
          &ProjectBoardGanttWidget::EmitDataChanged);
  connect(model_->ConstraintModel(), &KDGantt::ConstraintModel::constraintAdded, this,
          &ProjectBoardGanttWidget::EmitDataChanged);
  connect(model_->ConstraintModel(), &KDGantt::ConstraintModel::constraintRemoved, this,
          &ProjectBoardGanttWidget::EmitDataChanged);
}

ProjectBoardGanttWidget::~ProjectBoardGanttWidget() = default;

void ProjectBoardGanttWidget::LoadFromJson(const QJsonObject& board) {
  loading_ = true;
  model_->LoadFromJson(board);
  view_->expandAll();
  loading_ = false;
}

auto ProjectBoardGanttWidget::ToJson() const -> QJsonObject {
  return model_->ToJson();
}

auto ProjectBoardGanttWidget::Model() const -> ProjectBoardGanttModel* {
  return model_.get();
}

void ProjectBoardGanttWidget::EmitDataChanged() {
  if (loading_) {
    return;
  }
  emit DataChanged(model_->ToJson());
}

}  // namespace cppwiki::gui::project_board::gantt
