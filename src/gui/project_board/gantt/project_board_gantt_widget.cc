#include "gui/project_board/gantt/project_board_gantt_widget.h"

#include <KDGanttConstraintModel>
#include <KDGanttDateTimeGrid>
#include <KDGanttGraphicsView>
#include <KDGanttItemDelegate>
#include <KDGanttProxyModel>
#include <KDGanttView>
#include <QComboBox>
#include <QEvent>
#include <QFileDialog>
#include <QFrame>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPageLayout>
#include <QPrinter>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "gui/project_board/gantt/project_board_gantt_critical_path.h"
#include "gui/project_board/gantt/project_board_gantt_item_delegate.h"
#include "gui/project_board/gantt/project_board_gantt_model.h"

namespace cppwiki::gui::project_board::gantt {

// Fixed-height variant of the default tree item delegate: KDGantt's left tree view and its
// right-hand chart share row geometry via TreeViewRowController, which reads row heights straight
// off the tree view's own visualRect() -- so changing the *tree view's* row height (via its item
// delegate's sizeHint(), same as any QAbstractItemView) is all "compact mode" needs on the tree
// side. paint()/createEditor() are inherited unchanged, so inline task-name editing keeps working.
class RowHeightDelegate : public QStyledItemDelegate {
 public:
  explicit RowHeightDelegate(int row_height, QObject* parent = nullptr)
      : QStyledItemDelegate(parent), row_height_(row_height) {}

  void SetRowHeight(int row_height) {
    row_height_ = row_height;
  }

  [[nodiscard]] auto sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
      -> QSize override {
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(row_height_);
    return size;
  }

 private:
  int row_height_;
};

namespace {

// Number of scene units per day in the Gantt header at the Day scale. KDGantt defaults to ~30,
// which makes multi-day task bars look like thin slivers at typical window sizes; 80 gives each
// day enough horizontal room that bars, their labels, and dependency arrows remain readable
// without excessive horizontal scrolling.
constexpr qreal kDayWidth = 80.0;

// Base day-width picked for each scale so its columns start out at a readable size -- the same
// dayWidth that looks right for day columns (80px) would render hour columns at ~3px (80/24) and
// month columns at over 2000px (80*~30), so each scale gets its own starting point. The zoom
// in/out buttons then scale relative to whichever of these is currently active (see
// HandleZoomIn()/HandleZoomOut()). Order matches the entries added to scale_combo_ below.
constexpr qreal kHourScaleDayWidth = 720.0;  // 30px/hour
constexpr qreal kWeekScaleDayWidth = 20.0;   // 140px/week
constexpr qreal kMonthScaleDayWidth = 6.0;   // ~180px/30-day month

constexpr qreal kMinDayWidth = 4.0;
constexpr qreal kMaxDayWidth = 3000.0;
constexpr qreal kZoomFactor = 1.25;

constexpr int kNormalRowHeight = 30;
constexpr int kCompactRowHeight = 18;

// Same fill colors ProjectBoardGanttWidget's theme block hands to the item delegate (lifted from
// @svar-ui/react-gantt's WillowDark theme) -- named here too so the legend can draw swatches that
// actually match what's on the chart, instead of a second, driftable copy of the same colors.
constexpr QColor kTaskColor(0x09, 0x8c, 0xdc);
constexpr QColor kSummaryColor(0x09, 0x9f, 0x81);
constexpr QColor kMilestoneColor(0xad, 0x44, 0xab);

auto MakeLegendSwatch(const QColor& color, const QString& label) -> QWidget* {
  auto* container = new QWidget;
  auto* row = new QHBoxLayout(container);
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(4);

  auto* swatch = new QFrame;
  swatch->setFixedSize(10, 10);
  swatch->setStyleSheet(
      QStringLiteral("background-color: %1; border-radius: 2px;").arg(color.name()));

  auto* text = new QLabel(label);

  row->addWidget(swatch);
  row->addWidget(text);
  return container;
}

}  // namespace

ProjectBoardGanttWidget::ProjectBoardGanttWidget(QWidget* parent)
    : QWidget(parent),
      view_(new KDGantt::View(this)),
      model_(std::make_unique<ProjectBoardGanttModel>()) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  // Stretch factor 1: with the toolbar inserted below (stretch 0, see the "Toolbar" block), both
  // would otherwise have equal (0) stretch -- QVBoxLayout then splits leftover space between
  // Preferred-policy widgets roughly evenly rather than leaving it unclaimed, which is what grew
  // the toolbar to half the window's height instead of just its one row of controls. Giving only
  // view_ a nonzero stretch makes it the sole claimant of all space beyond each widget's own
  // size hint.
  layout->addWidget(view_, 1);

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
      day_width_ = kDayWidth;
      grid->setDayWidth(day_width_);

      // DateTimeGrid already shades Saturday/Sunday by default (its freeDays set defaults to
      // {Saturday, Sunday} -- see kdganttdatetimegrid_p.h), but only when freeDaysBrush is left
      // at its own default (an empty QBrush) does it fall back to QPalette::midlight(), a role
      // this app's dark theme never customizes (it themes via stylesheet, not QPalette), so the
      // weekend band renders in Qt's stock light gray -- readable, but visibly foreign next to
      // everything else in this dark chart. Giving it an explicit, subtle themed brush instead of
      // relying on that fallback keeps the shading but makes it match.
      grid->setFreeDaysBrush(QColor(0xff, 0xff, 0xff, 0x0c));
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

  // The left panel's tree view has exactly one column ("Task"); left at QHeaderView's default,
  // that column keeps whatever width it was given (or its content-derived size), leaving a strip
  // of dead space to the right of task names whenever the panel is wider than the longest name.
  // Stretching the (only, so also last) section makes it fill the panel instead.
  if (auto* tree_view = qobject_cast<QTreeView*>(view_->leftView())) {
    tree_view->header()->setStretchLastSection(true);
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

    delegate->setDefaultBrush(KDGantt::TypeTask, kTaskColor);  // --wx-gantt-task-fill-color
    delegate->setDefaultBrush(KDGantt::TypeSummary,
                              kSummaryColor);  // --wx-gantt-summary-fill-color
    delegate->setDefaultBrush(KDGantt::TypeEvent, kMilestoneColor);  // --wx-gantt-milestone-color
  }

  // Row height is normally whatever the tree view's default delegate/font metrics produce;
  // installing this one up front (at the normal height) means HandleCompactToggled() only ever
  // has to change its row_height_ and force a relayout, not swap delegates at runtime.
  if (auto* tree_view = qobject_cast<QTreeView*>(view_->leftView())) {
    row_height_delegate_ = new RowHeightDelegate(kNormalRowHeight, tree_view);
    tree_view->setItemDelegate(row_height_delegate_);
  }

  // --- Toolbar: zoom, compact rows, legend, PDF export -------------------------
  {
    auto* toolbar = new QWidget(this);
    auto* toolbar_layout = new QHBoxLayout(toolbar);
    toolbar_layout->setContentsMargins(4, 4, 4, 4);

    scale_combo_ = new QComboBox(toolbar);
    scale_combo_->addItem(tr("Hour"), static_cast<int>(KDGantt::DateTimeGrid::ScaleHour));
    scale_combo_->addItem(tr("Day"), static_cast<int>(KDGantt::DateTimeGrid::ScaleDay));
    scale_combo_->addItem(tr("Week"), static_cast<int>(KDGantt::DateTimeGrid::ScaleWeek));
    scale_combo_->addItem(tr("Month"), static_cast<int>(KDGantt::DateTimeGrid::ScaleMonth));
    scale_combo_->setCurrentIndex(1);  // Day, matching the grid's initial setup above.
    connect(scale_combo_, &QComboBox::currentIndexChanged, this,
            &ProjectBoardGanttWidget::HandleScaleChanged);
    toolbar_layout->addWidget(scale_combo_);
    // See eventFilter()'s QEvent::Show case for why: scale_combo_'s effective style() ends up a
    // QStyleSheetStyle wrapping the real qlementine style (an ancestor -- MainWindow's
    // current_content_widget_ -- gets cppwiki.qss applied for its own background rule), which
    // silently disables qlementine's own popup-geometry fix-up, so it has to be redone here.
    scale_combo_->view()->window()->installEventFilter(this);

    auto* zoom_out = new QToolButton(toolbar);
    zoom_out->setText(QStringLiteral("−"));  // U+2212 MINUS SIGN
    zoom_out->setToolTip(tr("Zoom out"));
    connect(zoom_out, &QToolButton::clicked, this, &ProjectBoardGanttWidget::HandleZoomOut);
    toolbar_layout->addWidget(zoom_out);

    auto* zoom_in = new QToolButton(toolbar);
    zoom_in->setText(QStringLiteral("+"));
    zoom_in->setToolTip(tr("Zoom in"));
    connect(zoom_in, &QToolButton::clicked, this, &ProjectBoardGanttWidget::HandleZoomIn);
    toolbar_layout->addWidget(zoom_in);

    auto* compact_toggle = new QToolButton(toolbar);
    compact_toggle->setText(tr("Compact"));
    compact_toggle->setCheckable(true);
    compact_toggle->setToolTip(tr("Shrink row height to fit more rows on screen"));
    connect(compact_toggle, &QToolButton::toggled, this,
            &ProjectBoardGanttWidget::HandleCompactToggled);
    toolbar_layout->addWidget(compact_toggle);

    auto* critical_path_toggle = new QToolButton(toolbar);
    critical_path_toggle->setText(tr("Critical path"));
    critical_path_toggle->setCheckable(true);
    critical_path_toggle->setToolTip(
        tr("Highlight the chain of tasks that determines the project's overall duration"));
    connect(critical_path_toggle, &QToolButton::toggled, this,
            &ProjectBoardGanttWidget::HandleCriticalPathToggled);
    toolbar_layout->addWidget(critical_path_toggle);

    toolbar_layout->addSpacing(12);
    toolbar_layout->addWidget(MakeLegendSwatch(kTaskColor, tr("Task")));
    toolbar_layout->addWidget(MakeLegendSwatch(kSummaryColor, tr("Summary")));
    toolbar_layout->addWidget(MakeLegendSwatch(kMilestoneColor, tr("Milestone")));

    toolbar_layout->addStretch(1);

    auto* export_pdf = new QPushButton(tr("Export PDF…"), toolbar);
    connect(export_pdf, &QPushButton::clicked, this, &ProjectBoardGanttWidget::ExportToPdf);
    toolbar_layout->addWidget(export_pdf);

    layout->insertWidget(0, toolbar);
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
  // ProjectBoardGanttModel::LoadFromJson() themes every link it loads from a document (white
  // pen, see LinkPen()/InvalidLinkPen()) -- but a link the user creates interactively (dragging
  // from one bar's connector dot to another) is added straight to the ConstraintModel by
  // KDGantt's own mouse handling, as a bare Constraint with no pen data at all, so it falls back
  // to KDGantt::ItemDelegate's hardcoded black/red. Apply the same themed pen here so a
  // freshly-drawn link looks the same as one that survived a save/reload round trip instead of
  // only matching after the document is saved and the app restarted.
  connect(model_->ConstraintModel(), &KDGantt::ConstraintModel::constraintAdded, this,
          [this](const KDGantt::Constraint& constraint) {
            if (constraint.data(KDGantt::Constraint::ValidConstraintPen).isValid()) {
              return;
            }
            // Deferred to the next event-loop iteration rather than done here, reentrant, inside
            // this very constraintAdded dispatch: KDGantt::View keeps a *separate*, internally
            // address-mapped ConstraintModel for rendering (see View::setConstraintModel()), and
            // the relay that mirrors an edit here back onto that mapped model runs synchronously,
            // nested inside this same signal dispatch. KDGantt::GraphicsScene's own listener for
            // the *same original* constraintAdded is connected after this lambda's -- Qt still
            // has to invoke it once dispatch reaches that point, and by then it creates its own
            // untamed ConstraintGraphicsItem for the same pair, unaware this lambda's nested
            // remove/add already swapped in a themed one earlier in the SAME dispatch. Two
            // overlapping items result (one white, one black on top), and which one paints varies
            // frame to frame. Running the swap only after the current dispatch (and GraphicsScene's
            // own listener) has fully unwound avoids that reentrant double-creation.
            const KDGantt::Constraint captured = constraint;
            QTimer::singleShot(0, this, [this, captured]() {
              for (const auto& current : model_->ConstraintModel()->constraints()) {
                if (!current.compareIndexes(captured)) {
                  continue;
                }
                if (current.data(KDGantt::Constraint::ValidConstraintPen).isValid()) {
                  return;  // already themed (e.g. by a reload that happened in the meantime)
                }
                KDGantt::Constraint themed = current;
                themed.setData(KDGantt::Constraint::ValidConstraintPen,
                               QVariant::fromValue(model_->LinkPen()));
                themed.setData(KDGantt::Constraint::InvalidConstraintPen,
                               QVariant::fromValue(model_->LinkPen()));
                // Guard with loading_ (the same flag LoadFromJson() and the critical-path
                // recompute use) so this bookkeeping swap -- not itself a second user edit --
                // doesn't double-count as an extra DataChanged emission.
                loading_ = true;
                model_->ConstraintModel()->removeConstraint(current);
                model_->ConstraintModel()->addConstraint(themed);
                loading_ = false;
                return;
              }
              // No match: the link was removed (e.g. toggled off again) before this callback got
              // a chance to run -- nothing to theme.
            });
          });
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

auto ProjectBoardGanttWidget::View() const -> KDGantt::View* {
  return view_;
}

bool ProjectBoardGanttWidget::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::Show && watched == scale_combo_->view()->window()) {
    // QComboBox's own popup-height calculation undercounts the room below the popup by the
    // current screen's own y-offset within the virtual desktop, on any screen that isn't
    // anchored at global y=0 (e.g. a second monitor placed below/right of another) -- it
    // collapses the popup to a sliver of only a couple of rows, with the rest hidden behind a
    // scroll affordance, even though there's plenty of actual room below it on that screen.
    // qlementine's own style ships a fix for the equivalent bug in its own popup-geometry
    // fix-up (ComboboxItemViewFilter::fixViewGeometry()), but that code never runs here: an
    // ancestor (MainWindow's current_content_widget_) has cppwiki.qss applied to it for an
    // unrelated background rule, which wraps every descendant's effective style() -- including
    // scale_combo_'s -- in a QStyleSheetStyle, and that fix-up bails out unless
    // qobject_cast<QlementineStyle*>(comboBox->style()) succeeds. Recomputing the correct
    // available height here and growing the popup back out (only up to what's actually needed
    // and actually available) works regardless of which style ends up wrapping it.
    auto* view = scale_combo_->view();
    // Width: the same fix-up that's bypassed above also normally widens the popup to fit its
    // widest item (view->sizeHintForColumn(0)) -- without it, the popup stays exactly as narrow
    // as the closed combo box itself (just enough for the checkmark indicator), which pushes
    // every item's label completely outside the visible rect instead of merely truncating it.
    const auto needed_width = view->sizeHintForColumn(0);
    if (needed_width > view->width()) {
      view->setFixedWidth(needed_width);
    }
    if (auto* screen = view->screen()) {
      const auto view_global_y = view->mapToGlobal(QPoint(0, 0)).y();
      const auto available_height = screen->geometry().y() + screen->geometry().height() - 24 - view_global_y;
      int needed_height = 0;
      for (int row = 0; row < scale_combo_->count(); ++row) {
        needed_height += view->sizeHintForRow(row);
      }
      const auto wanted_height = needed_height < view->height() ? view->height() : needed_height;
      const auto corrected_height =
          wanted_height < available_height ? wanted_height : available_height;
      if (corrected_height > view->height()) {
        view->setFixedHeight(corrected_height);
      }
    }
    view->window()->adjustSize();
  }
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

void ProjectBoardGanttWidget::HandleScaleChanged(int index) {
  auto* grid = qobject_cast<KDGantt::DateTimeGrid*>(view_->grid());
  if (grid == nullptr) {
    return;
  }
  const auto scale =
      static_cast<KDGantt::DateTimeGrid::Scale>(scale_combo_->itemData(index).toInt());
  grid->setScale(scale);
  switch (scale) {
    case KDGantt::DateTimeGrid::ScaleHour:
      ApplyDayWidth(kHourScaleDayWidth);
      break;
    case KDGantt::DateTimeGrid::ScaleWeek:
      ApplyDayWidth(kWeekScaleDayWidth);
      break;
    case KDGantt::DateTimeGrid::ScaleMonth:
      ApplyDayWidth(kMonthScaleDayWidth);
      break;
    case KDGantt::DateTimeGrid::ScaleDay:
    case KDGantt::DateTimeGrid::ScaleAuto:
    default:
      ApplyDayWidth(kDayWidth);
      break;
  }
}

void ProjectBoardGanttWidget::HandleZoomIn() {
  ApplyDayWidth(day_width_ * kZoomFactor);
}

void ProjectBoardGanttWidget::HandleZoomOut() {
  ApplyDayWidth(day_width_ / kZoomFactor);
}

void ProjectBoardGanttWidget::ApplyDayWidth(qreal day_width) {
  auto* grid = qobject_cast<KDGantt::DateTimeGrid*>(view_->grid());
  if (grid == nullptr) {
    return;
  }
  day_width_ = qBound(kMinDayWidth, day_width, kMaxDayWidth);
  grid->setDayWidth(day_width_);
}

void ProjectBoardGanttWidget::HandleCompactToggled(bool checked) {
  if (row_height_delegate_ == nullptr) {
    return;
  }
  row_height_delegate_->SetRowHeight(checked ? kCompactRowHeight : kNormalRowHeight);
  auto* tree_view = qobject_cast<QTreeView*>(view_->leftView());
  if (tree_view != nullptr) {
    tree_view->doItemsLayout();
  }
  // TreeViewRowController (KDGantt's default row controller) reads each row's geometry straight
  // off the tree view's own visualRect() -- doItemsLayout() above makes that report the new
  // height immediately -- but GraphicsScene only re-queries that geometry when it (re)inserts
  // items, not on a plain row-height change with no accompanying model/expand-collapse event.
  // Re-attaching the same model (the same "full rebuild" hook LoadFromJson() already relies on)
  // forces every bar to be re-inserted against the now-current row heights.
  view_->setModel(model_.get());
  view_->setConstraintModel(model_->ConstraintModel());
  view_->expandAll();
}

void ProjectBoardGanttWidget::ExportToPdf() {
  const QString path = QFileDialog::getSaveFileName(this, tr("Export Gantt chart to PDF"),
                                                    QString(), tr("PDF files (*.pdf)"));
  if (path.isEmpty()) {
    return;
  }
  QPrinter printer(QPrinter::HighResolution);
  printer.setOutputFormat(QPrinter::PdfFormat);
  printer.setOutputFileName(path);
  printer.setPageOrientation(QPageLayout::Landscape);
  view_->print(&printer);
}

void ProjectBoardGanttWidget::HandleCriticalPathToggled(bool checked) {
  critical_path_enabled_ = checked;
  loading_ = true;
  model_->SetCriticalPathTaskIds(checked ? ComputeCriticalPath(model_->ToJson()).critical_task_ids
                                         : QSet<QString>());
  loading_ = false;
}

}  // namespace cppwiki::gui::project_board::gantt
