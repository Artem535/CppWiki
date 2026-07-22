#include "gui/project_board/table/project_task_table_widget.h"

#include <QHeaderView>
#include <QTableView>
#include <QVBoxLayout>

#include "gui/project_board/table/project_date_edit_delegate.h"
#include "gui/project_board/table/project_numeric_spin_delegate.h"
#include "gui/project_board/table/project_priority_pill_delegate.h"
#include "gui/project_board/table/project_status_pill_delegate.h"
#include "gui/project_board/table/project_task_table_model.h"

namespace cppwiki::gui::project_board {

namespace {

constexpr int kMaxDurationDays = 3650;
constexpr int kMaxProgressPercent = 100;

}  // namespace

ProjectTaskTableWidget::ProjectTaskTableWidget(QWidget* parent)
    : QWidget(parent), model_(new ProjectTaskTableModel(this)), view_(new QTableView(this)) {
  view_->setModel(model_);
  view_->setSortingEnabled(true);
  view_->setAlternatingRowColors(true);
  view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  view_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                         QAbstractItemView::AnyKeyPressed);
  view_->horizontalHeader()->setStretchLastSection(true);
  view_->horizontalHeader()->setSectionResizeMode(ProjectTaskTableModel::kTaskColumn,
                                                  QHeaderView::Stretch);
  view_->verticalHeader()->setVisible(false);

  view_->setItemDelegateForColumn(ProjectTaskTableModel::kStatusColumn,
                                  new ProjectStatusPillDelegate(view_));
  view_->setItemDelegateForColumn(ProjectTaskTableModel::kPriorityColumn,
                                  new ProjectPriorityPillDelegate(view_));
  view_->setItemDelegateForColumn(ProjectTaskTableModel::kStartColumn,
                                  new ProjectDateEditDelegate(view_));
  view_->setItemDelegateForColumn(
      ProjectTaskTableModel::kDurationColumn,
      new ProjectNumericSpinDelegate(0, kMaxDurationDays, QStringLiteral(" d"), view_));
  view_->setItemDelegateForColumn(
      ProjectTaskTableModel::kProgressColumn,
      new ProjectNumericSpinDelegate(0, kMaxProgressPercent, QStringLiteral("%"), view_));

  // Duration/Progress have no delegate-driven natural width and Qt's default column width is
  // narrower than their header text (e.g. "Duration (days)" truncates to "uration (day:" at the
  // default width) -- size every column to its header/content once up front. setDocument() below
  // is what actually populates rows, so this runs against whatever's in the model at construction
  // time (typically empty); resizeColumnsToContents() is called again at the end of setDocument()
  // once real data exists.
  view_->resizeColumnsToContents();

  connect(model_, &ProjectTaskTableModel::dataChanged, this,
          &ProjectTaskTableWidget::documentEdited);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(view_);
}

void ProjectTaskTableWidget::setDocument(const ProjectBoardDocument& document) {
  links_ = document.links;
  model_->setBoardColumns(document.columns);
  model_->setTasks(document.tasks);
  view_->resizeColumnsToContents();
}

ProjectBoardDocument ProjectTaskTableWidget::document() const {
  ProjectBoardDocument result;
  result.tasks = model_->tasks();
  result.columns = model_->boardColumns();
  result.links = links_;
  return result;
}

}  // namespace cppwiki::gui::project_board
