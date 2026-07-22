#include "gui/project_board/gantt/project_board_gantt_widget.h"

#include <KDGanttConstraintModel>
#include <KDGanttDateTimeGrid>
#include <KDGanttGraphicsView>
#include <KDGanttView>
#include <QGraphicsView>
#include <QSplitter>
#include <QVBoxLayout>

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
