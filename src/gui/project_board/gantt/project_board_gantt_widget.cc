#include "gui/project_board/gantt/project_board_gantt_widget.h"

#include <KDGanttConstraintModel>
#include <KDGanttDateTimeGrid>
#include <KDGanttGraphicsView>
#include <KDGanttItemDelegate>
#include <KDGanttProxyModel>
#include <KDGanttView>
#include <QEvent>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "gui/project_board/gantt/project_board_gantt_critical_path.h"
#include "gui/project_board/gantt/project_board_gantt_item_delegate.h"
#include "gui/project_board/gantt/project_board_gantt_model.h"

namespace cppwiki::gui::project_board::gantt {

namespace {

// Number of scene units per day in the Gantt header. KDGantt defaults to ~30, which makes
// multi-day task bars look like thin slivers at typical window sizes; 80 gives each day enough
// horizontal room that bars, their labels, and dependency arrows remain readable without
// excessive horizontal scrolling.
constexpr qreal kDayWidth = 80.0;

}  // namespace

ProjectBoardGanttWidget::ProjectBoardGanttWidget(QWidget* parent)
    : QWidget(parent),
      view_(new KDGantt::View(this)),
      model_(std::make_unique<ProjectBoardGanttModel>()) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  // --- Toolbar: critical path -----------------------------------------------
  {
    auto* toolbar = new QWidget(this);
    auto* toolbar_layout = new QHBoxLayout(toolbar);
    toolbar_layout->setContentsMargins(4, 4, 4, 4);

    auto* critical_path_toggle = new QToolButton(toolbar);
    critical_path_toggle->setText(tr("Critical path"));
    critical_path_toggle->setCheckable(true);
    critical_path_toggle->setToolTip(
        tr("Highlight the chain of tasks that determines the project's overall duration"));
    connect(critical_path_toggle, &QToolButton::toggled, this,
            &ProjectBoardGanttWidget::HandleCriticalPathToggled);
    toolbar_layout->addWidget(critical_path_toggle);
    toolbar_layout->addStretch(1);

    layout->addWidget(toolbar);
  }

  layout->addWidget(view_);

  // --- Rendering performance --------------------------------------------------
  //
  // KDGantt::GraphicsView (the right-hand Gantt chart area) hardcodes two
  // expensive defaults in its constructor (kdganttgraphicsview.cpp):
  //
  //   1. QGraphicsView::FullViewportUpdate — redraws the *entire* viewport on
  //      every interaction (drag, resize, scroll).  For a 1000×500 widget that's
  //      500k pixels per frame — needlessly expensive when only a single task bar
  //      (≈ 400×30 px) has actually moved.
  //
  //   2. QGraphicsScene::NoIndex — every frame does an O(n) linear scan of all
  //      items in the scene.  For a dynamic scene (items moving during drag) the
  //      BSP tree rebuild overhead actually makes BspTreeIndex *worse* than a
  //      linear scan (Qt docs: "if your scene uses many animations or is otherwise
  //      dynamic, the index itself will need to be updated frequently, and this
  //      can be a bottleneck"), so we keep NoIndex for the interactive case.
  //
  // Fixes (ordered by impact):
  //
  //  1. BoundingRectViewportUpdate — calculates the bounding rect of all items
  //     that have changed since the last frame and repaints only that single
  //     rectangle.  During a bar drag the changed area is the union of the old
  //     and new bar position; the rest of the chart (including all other bars,
  //     the grid background, and header labels) is preserved from the previous
  //     frame.  This is the single biggest FPS win.
  //
  //  2. CacheBackground — the DateTimeGrid's row-colour background (weekends,
  //     free days, "no information" fill) is painted to a cached pixmap and
  //     re-used across frames instead of being re-drawn on every paint.  The
  //     cache is invalidated automatically on scroll/resize.
  //
  //  3. DontSavePainterState / DontAdjustForAntialiasing — skip per-frame
  //     painter overhead that QGraphicsView normally adds for correctness.
  //
  //  4. ScaleDay — forces day-level Gantt headers so setDayWidth() is the
  //     authoritative horizontal zoom, not overridden by ScaleAuto heuristics.
  //
  //  5. kDayWidth — widens the Gantt timeline so task bars and their text
  //     labels are readable without zooming in.  At 80 px/day a 5-day task bar
  //     is 400 px wide — clearly visible at any window size > ~600 px.
  {
    auto* gv = view_->graphicsView();
    gv->setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    gv->setCacheMode(QGraphicsView::CacheBackground);
    gv->setOptimizationFlag(QGraphicsView::DontSavePainterState);
    gv->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing);
    if (auto* grid = qobject_cast<KDGantt::DateTimeGrid*>(view_->grid())) {
      grid->setScale(KDGantt::DateTimeGrid::ScaleDay);
      grid->setDayWidth(kDayWidth);
    }
  }

  // KDGantt::View's internal ProxyModel defaults to reading each Gantt role (ItemTypeRole,
  // StartTimeRole, etc.) from a *different column* of the source model (column 1, 2, 3, ...) --
  // a layout meant for a source model that dedicates one column per piece of Gantt data. Our
  // model is a single-column tree (see ProjectBoardGanttModel::LoadFromJson()): every role is
  // set directly on each task's column-0 item. Left at its defaults, every single lookup the
  // proxy makes reads a nonexistent column and returns an invalid QVariant -- which is why every
  // row was painted with the "no information" hatch pattern, and why task bars never had valid
  // geometry to render with (dates/type/completion all read as invalid). Remapping every role
  // onto column 0, read directly (no role indirection), fixes both.
  if (auto* proxy = qobject_cast<KDGantt::ProxyModel*>(view_->ganttProxyModel())) {
    for (int role :
         {static_cast<int>(KDGantt::ItemTypeRole), static_cast<int>(KDGantt::StartTimeRole),
          static_cast<int>(KDGantt::EndTimeRole), static_cast<int>(KDGantt::TaskCompletionRole),
          static_cast<int>(KDGantt::LegendRole)}) {
      proxy->setColumn(role, 0);
      proxy->setRole(role, role);
    }
  }

  view_->setModel(model_.get());
  view_->setConstraintModel(model_->ConstraintModel());

  // Give the Gantt chart area the lion's share of the horizontal space.  The
  // View's internal QSplitter defaults to equal splits; the left tree-view
  // panel doesn't need more than ~200 px for task names, while the chart needs
  // everything else for the timeline.
  if (auto* sp = view_->splitter()) {
    sp->setSizes({200, 800});
    // Opaque (live) resize repaints the GraphicsView on every mouse-move event during a drag —
    // for a splitter between the task-name tree and the Gantt chart, that means a full
    // updateSceneRect()/repaint per pixel dragged, which is what made panel resizing feel
    // stuttery. Non-opaque resize shows a plain divider line while dragging and only resizes
    // (and repaints) the actual widgets once, on release.
    sp->setOpaqueResize(false);
  }

  // --- Visual theme -----------------------------------------------------------
  //
  // KDGantt::ItemDelegate (kdganttitemdelegate.cpp) hardcodes Qt::black for the outline pen of
  // every bar type *and* reuses that same pen to draw the task-name label text beside each bar --
  // invisible against this app's dark theme, whose chart-area background comes from the ordinary
  // (dark) QPalette::Base rather than anything KDGantt paints itself. It also draws every bar as a
  // plain sharp-cornered rectangle (summaries as a pointed bracket), with no shadow -- noticeably
  // flatter than the web Project board's Gantt tab
  // (frontend/editor/src/project/ProjectBoardView.tsx's SVAR react-gantt, `WillowDark` theme),
  // which uses rounded 3px bars with a soft drop shadow. ProjectBoardGanttItemDelegate (see its
  // header) carries the shape the rest of the way; colors are still set here via the same
  // setDefaultBrush()/setDefaultPen() API, lifted directly from
  // @svar-ui/react-gantt/dist/index.css's `.wx-willow-dark-theme` custom-property block.
  auto* delegate = new ProjectBoardGanttItemDelegate(view_);
  view_->setItemDelegate(delegate);
  {
    const QColor kLightText(0xff, 0xff, 0xff, 0xe5);  // --wx-gantt-task/summary-font-color
    const QPen text_pen(kLightText, 1.);
    delegate->setDefaultPen(KDGantt::TypeTask, text_pen);
    delegate->setDefaultPen(KDGantt::TypeSummary, text_pen);
    delegate->setDefaultPen(KDGantt::TypeEvent, text_pen);

    delegate->setDefaultBrush(KDGantt::TypeTask,
                              QColor(0x09, 0x8c, 0xdc));  // --wx-gantt-task-fill-color
    delegate->setDefaultBrush(KDGantt::TypeSummary,
                              QColor(0x09, 0x9f, 0x81));  // --wx-gantt-summary-fill-color
    delegate->setDefaultBrush(KDGantt::TypeEvent,
                              QColor(0xad, 0x44, 0xab));  // --wx-gantt-milestone-color
  }

  // Plain wheel scrolling normally drives Qt's default vertical scrollbar only; redirect it to
  // the chart's horizontal scrollbar instead, since panning the timeline left/right is the far
  // more common gesture for a Gantt chart than scrolling through rows (a Shift+wheel or the
  // scrollbar itself still reaches the vertical axis for boards with enough rows to overflow).
  view_->graphicsView()->viewport()->installEventFilter(this);

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
  // ProjectBoardGanttModel::LoadFromJson() batches its row insertions per parent (see its own
  // comment) specifically so this call only makes the already-attached KDGantt view rebuild its
  // scene a handful of times (once per distinct parent) instead of once per task -- see #119.
  model_->LoadFromJson(board);
  view_->expandAll();
  // The new model's rows start with no critical-path flag at all (not even "false") -- reapply
  // the highlight now if the toggle was already on, rather than leaving the newly loaded document
  // unhighlighted until the next edit or a manual toggle.
  if (critical_path_enabled_) {
    model_->SetCriticalPathTaskIds(ComputeCriticalPath(model_->ToJson()).critical_task_ids);
  }
  loading_ = false;
}

auto ProjectBoardGanttWidget::ToJson() const -> QJsonObject {
  return model_->ToJson();
}

auto ProjectBoardGanttWidget::Model() const -> ProjectBoardGanttModel* {
  return model_.get();
}

bool ProjectBoardGanttWidget::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::Wheel && watched == view_->graphicsView()->viewport()) {
    auto* wheel_event = static_cast<QWheelEvent*>(event);
    if (!(wheel_event->modifiers() & Qt::ShiftModifier)) {
      auto* horizontal_scrollbar = view_->graphicsView()->horizontalScrollBar();
      horizontal_scrollbar->setValue(horizontal_scrollbar->value() - wheel_event->angleDelta().y());
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void ProjectBoardGanttWidget::EmitDataChanged() {
  if (loading_) {
    return;
  }
  // Keep the highlight in sync with whatever just changed (a dragged bar, an added/removed
  // dependency link) instead of freezing it at whatever it was when the toggle was turned on.
  if (critical_path_enabled_) {
    loading_ = true;
    model_->SetCriticalPathTaskIds(ComputeCriticalPath(model_->ToJson()).critical_task_ids);
    loading_ = false;
  }
  emit DataChanged(model_->ToJson());
}

void ProjectBoardGanttWidget::HandleCriticalPathToggled(bool checked) {
  critical_path_enabled_ = checked;
  loading_ = true;
  model_->SetCriticalPathTaskIds(checked ? ComputeCriticalPath(model_->ToJson()).critical_task_ids
                                         : QSet<QString>());
  loading_ = false;
}

}  // namespace cppwiki::gui::project_board::gantt
