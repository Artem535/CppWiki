#ifndef CPPWIKI_BENCHMARKS_KDGANTT_BENCHMARK_SYNTHETIC_TASK_MODEL_H_
#define CPPWIKI_BENCHMARKS_KDGANTT_BENCHMARK_SYNTHETIC_TASK_MODEL_H_

#include <QJsonObject>
#include <QStandardItemModel>
#include <QString>

namespace KDGantt {
class ConstraintModel;
}  // namespace KDGantt

namespace cppwiki::gui::project_board::gantt::benchmark {

struct DatasetParams {
  int taskCount = 1000;
  int maxDepth = 2;
  int dependencyCount = 0;
  double summaryRatio = 0.1;
  double milestoneRatio = 0.05;
  int daySpan = 365;
  int minDurationDays = 1;
  int maxDurationDays = 30;
};

QJsonObject BuildSyntheticBoard(const DatasetParams& params);

void PopulateModel(QStandardItemModel* model, KDGantt::ConstraintModel* constraintModel,
                   const DatasetParams& params);

enum class TestCase {
  ValidTask,
  MissingStart,
  MissingEnd,
  ZeroDurationTask,
  EndBeforeStart,
  OnePixelTask,
  SummaryNoChildren,
  TypeMulti,
  TaskBeforeGrid,
  TaskAfterGrid,
  DstBoundary,
};

void InjectTestCase(QStandardItemModel* model, TestCase tc, int row = 0);

}  // namespace cppwiki::gui::project_board::gantt::benchmark

#endif  // CPPWIKI_BENCHMARKS_KDGANTT_BENCHMARK_SYNTHETIC_TASK_MODEL_H_