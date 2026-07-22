#ifndef CPPWIKI_BENCHMARKS_KDGANTT_BENCHMARK_GANTT_DIAGNOSTIC_H_
#define CPPWIKI_BENCHMARKS_KDGANTT_BENCHMARK_GANTT_DIAGNOSTIC_H_

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QString>
#include <QStringList>

namespace cppwiki::gui::project_board::gantt::diagnostic {

struct SignalCounters {
  int dataChanged = 0;
  int rowsInserted = 0;
  int rowsRemoved = 0;
  int layoutChanged = 0;
  int modelReset = 0;
};

struct VisibilityResult {
  bool visible = false;
  QStringList reasons;
};

QStringList ValidateGanttIndex(const QModelIndex& index);

QString DumpGanttRoles(const QModelIndex& index);

QStringList DumpModelTree(QAbstractItemModel* model, const QModelIndex& parent = QModelIndex(),
                          int depth = 0);

QStringList ValidateModelTree(QAbstractItemModel* model, const QModelIndex& parent = QModelIndex(),
                              const QString& path = QString());

void InstallSignalProbe(QAbstractItemModel* model, SignalCounters* counters);

VisibilityResult CheckTaskVisibility(const QModelIndex& index);

QString GenerateReport(QAbstractItemModel* model, const QString& label);

}  // namespace cppwiki::gui::project_board::gantt::diagnostic

#endif  // CPPWIKI_BENCHMARKS_KDGANTT_BENCHMARK_GANTT_DIAGNOSTIC_H_