#include <KDGanttConstraintModel>
#include <KDGanttDateTimeGrid>
#include <KDGanttGlobal>
#include <KDGanttGraphicsView>
#include <KDGanttView>
#include <QAbstractProxyModel>
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QStandardItemModel>
#include <QString>
#include <QTextStream>

#include "gantt_diagnostic.h"
#include "gui/project_board/gantt/project_board_gantt_model.h"
#include "gui/project_board/gantt/project_board_gantt_widget.h"
#include "synthetic_task_model.h"

namespace {

using namespace cppwiki::gui::project_board::gantt;

auto MakeLabel(const QString& name, int taskCount) -> QString {
  return QStringLiteral("%1 (n=%2)").arg(name).arg(taskCount);
}

struct BenchmarkResult {
  QString label;
  qint64 loadMs = 0;
  qint64 expandMs = 0;
  qint64 validateMs = 0;
  int errorCount = 0;
};

auto RunBenchmark(const QString& name, int taskCount) -> BenchmarkResult {
  BenchmarkResult result;
  result.label = MakeLabel(name, taskCount);

  benchmark::DatasetParams params;
  params.taskCount = taskCount;
  params.maxDepth = 2;
  params.dependencyCount = taskCount / 10;
  params.summaryRatio = 0.1;
  params.milestoneRatio = 0.05;

  auto* model = new ProjectBoardGanttModel();
  KDGantt::View view;
  view.setModel(model);
  view.setConstraintModel(model->ConstraintModel());

  QElapsedTimer timer;

  // Strategy 1: LoadFromJson (full round-trip)
  timer.start();
  const auto board = benchmark::BuildSyntheticBoard(params);
  model->LoadFromJson(board);
  result.loadMs = timer.nsecsElapsed() / 1000000;

  // Expand all
  timer.start();
  view.expandAll();
  result.expandMs = timer.nsecsElapsed() / 1000000;

  // Validate
  timer.start();
  const auto errors = diagnostic::ValidateModelTree(model);
  result.validateMs = timer.nsecsElapsed() / 1000000;
  result.errorCount = static_cast<int>(errors.size());

  delete model;
  return result;
}

void PrintBenchmarkTable(const QVector<BenchmarkResult>& results, QTextStream& out) {
  out << "## Load Time Benchmark\n\n";
  out << "| Dataset | Load (ms) | Expand (ms) | Validate (ms) | Errors |\n";
  out << "|---------|-----------|-------------|---------------|--------|\n";
  for (const auto& r : results) {
    out << QStringLiteral("| %1 | %2 | %3 | %4 | %5 |\n")
               .arg(r.label)
               .arg(r.loadMs)
               .arg(r.expandMs)
               .arg(r.validateMs)
               .arg(r.errorCount);
  }
  out << "\n";
}

void PrintEdgeCaseResults(QTextStream& out) {
  using namespace benchmark;

  out << "## Edge Case Validation\n\n";

  std::initializer_list<std::pair<TestCase, const char*>> cases = {
      {TestCase::ValidTask, "ValidTask"},
      {TestCase::MissingStart, "MissingStart"},
      {TestCase::MissingEnd, "MissingEnd"},
      {TestCase::ZeroDurationTask, "ZeroDurationTask"},
      {TestCase::EndBeforeStart, "EndBeforeStart"},
      {TestCase::OnePixelTask, "OnePixelTask"},
      {TestCase::SummaryNoChildren, "SummaryNoChildren"},
      {TestCase::TypeMulti, "TypeMulti"},
      {TestCase::TaskBeforeGrid, "TaskBeforeGrid"},
      {TestCase::TaskAfterGrid, "TaskAfterGrid"},
      {TestCase::DstBoundary, "DstBoundary"},
  };

  for (const auto& [tc, name] : cases) {
    auto* model = new QStandardItemModel();
    InjectTestCase(model, tc);
    auto* item = model->item(0);
    QModelIndex idx;
    if (item != nullptr) {
      idx = model->indexFromItem(item);
    }

    out << QStringLiteral("### %1\n\n").arg(name);
    if (idx.isValid()) {
      out << QStringLiteral("- Roles: %1\n").arg(diagnostic::DumpGanttRoles(idx));
      const auto errors = diagnostic::ValidateGanttIndex(idx);
      if (errors.isEmpty()) {
        out << QStringLiteral("- Validation: PASS\n");
      } else {
        out << QStringLiteral("- Validation: FAIL\n");
        for (const auto& err : errors) {
          out << QStringLiteral("  - %1\n").arg(err);
        }
      }
      const auto visibility = diagnostic::CheckTaskVisibility(idx);
      out << QStringLiteral("- Visible: %1\n").arg(visibility.visible ? "yes" : "no");
      if (!visibility.visible) {
        for (const auto& reason : visibility.reasons) {
          out << QStringLiteral("  - Reason: %1\n").arg(reason);
        }
      }
    } else {
      out << QStringLiteral("- Index: (invalid)\n");
    }
    out << "\n";
    delete model;
  }
}

void PrintProxyChainDump(QTextStream& out) {
  out << "## Proxy Chain Role Dump\n\n";

  auto* model = new ProjectBoardGanttModel();
  KDGantt::View view;
  view.setModel(model);
  view.setConstraintModel(model->ConstraintModel());

  benchmark::DatasetParams params;
  params.taskCount = 10;
  params.maxDepth = 1;
  const auto board = benchmark::BuildSyntheticBoard(params);
  model->LoadFromJson(board);
  view.expandAll();

  // Walk the KDGantt proxy chain
  auto* ganttProxy = view.ganttProxyModel();
  auto* gv = view.graphicsView();
  auto* summaryModel = gv != nullptr ? gv->summaryHandlingModel() : nullptr;

  out << "### Source model (ProjectBoardGanttModel)\n\n";
  out << QStringLiteral("```\n%1\n```\n\n").arg(diagnostic::GenerateReport(model, "Source"));

  out << "### KDGantt::ProxyModel (ganttProxyModel)\n\n";
  if (ganttProxy != nullptr) {
    out << QStringLiteral("```\n%1\n```\n\n").arg(diagnostic::GenerateReport(ganttProxy, "Proxy"));
  } else {
    out << QStringLiteral("(null)\n\n");
  }

  out << "### SummaryHandlingProxyModel\n\n";
  if (summaryModel != nullptr) {
    out << QStringLiteral("```\n%1\n```\n\n")
               .arg(diagnostic::GenerateReport(summaryModel, "SummaryHandling"));
  } else {
    out << QStringLiteral("(null)\n\n");
  }

  delete model;
}

void PrintReport(const QVector<BenchmarkResult>& results, QTextStream& out) {
  out << "# KDGantt Integration Diagnostic Report\n\n";
  out << QStringLiteral("Generated: %1\n\n")
             .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
  out << QStringLiteral("KDGantt version: built from KDChart (see third_party/KDChart)\n");
  out << QStringLiteral(
             "KDGantt roles: StartTime=%1, EndTime=%2, TaskCompletion=%3, "
             "ItemType=%4\n\n")
             .arg(KDGantt::StartTimeRole)
             .arg(KDGantt::EndTimeRole)
             .arg(KDGantt::TaskCompletionRole)
             .arg(KDGantt::ItemTypeRole);

  PrintBenchmarkTable(results, out);
  PrintEdgeCaseResults(out);
  PrintProxyChainDump(out);
}

}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);

  QTextStream out(stdout);

  // Run benchmarks
  QVector<BenchmarkResult> results;
  results.push_back(RunBenchmark("Small", 100));
  results.push_back(RunBenchmark("Medium", 1000));
  results.push_back(RunBenchmark("Large", 5000));

  // Print the full report
  PrintReport(results, out);

  return 0;
}