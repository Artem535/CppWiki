#include "gantt_diagnostic.h"

#include <KDGanttConstraintModel>
#include <KDGanttGlobal>
#include <QAbstractProxyModel>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QMetaObject>
#include <QStandardItemModel>
#include <QtGlobal>

namespace cppwiki::gui::project_board::gantt::diagnostic {

namespace {

auto ItemTypeName(int type) -> QString {
  switch (type) {
    case KDGantt::TypeNone:
      return QStringLiteral("None");
    case KDGantt::TypeEvent:
      return QStringLiteral("Event");
    case KDGantt::TypeTask:
      return QStringLiteral("Task");
    case KDGantt::TypeSummary:
      return QStringLiteral("Summary");
    case KDGantt::TypeMulti:
      return QStringLiteral("Multi");
    default:
      return QStringLiteral("Custom(%1)").arg(type);
  }
}

}  // namespace

QStringList ValidateGanttIndex(const QModelIndex& index) {
  QStringList errors;
  if (!index.isValid()) {
    errors << QStringLiteral("Index is invalid");
    return errors;
  }

  const auto type = static_cast<KDGantt::ItemType>(index.data(KDGantt::ItemTypeRole).toInt());
  if (type == KDGantt::TypeNone) {
    errors << QStringLiteral("ItemTypeRole is TypeNone (0)");
  } else if (type < KDGantt::TypeNone || type > KDGantt::TypeUser) {
    errors << QStringLiteral("ItemTypeRole has out-of-range value %1").arg(static_cast<int>(type));
  }

  const auto start = index.data(KDGantt::StartTimeRole).toDateTime();
  if (!start.isValid()) {
    errors << QStringLiteral("StartTimeRole is missing or invalid");
  }

  const auto end = index.data(KDGantt::EndTimeRole).toDateTime();
  if (!end.isValid()) {
    errors << QStringLiteral("EndTimeRole is missing or invalid");
  }

  if (start.isValid() && end.isValid() && end < start) {
    errors << QStringLiteral("EndTime (%1) is before StartTime (%2)")
                  .arg(end.toString(Qt::ISODate), start.toString(Qt::ISODate));
  }

  const auto completion = index.data(KDGantt::TaskCompletionRole).toInt();
  if (completion < 0 || completion > 100) {
    errors << QStringLiteral("TaskCompletionRole is out of range: %1 (expected 0-100)")
                  .arg(completion);
  }

  if (type == KDGantt::TypeSummary) {
    if (index.model() && index.model()->rowCount(index) == 0) {
      errors << QStringLiteral("ItemType is Summary but has no children");
    }
  }

  if (type == KDGantt::TypeMulti) {
    errors << QStringLiteral("ItemType is TypeMulti — not supported by KDGantt rendering");
  }

  return errors;
}

QString DumpGanttRoles(const QModelIndex& index) {
  if (!index.isValid()) {
    return QStringLiteral("(invalid)");
  }

  QStringList parts;
  parts << QStringLiteral("row=%1 col=%2").arg(index.row()).arg(index.column());

  const auto display = index.data(Qt::DisplayRole).toString();
  parts << QStringLiteral("display=\"%1\"").arg(display);

  const auto type = static_cast<KDGantt::ItemType>(index.data(KDGantt::ItemTypeRole).toInt());
  parts << QStringLiteral("type=%1").arg(ItemTypeName(static_cast<int>(type)));

  const auto start = index.data(KDGantt::StartTimeRole).toDateTime();
  parts << QStringLiteral("start=%1")
               .arg(start.isValid() ? start.toString(Qt::ISODate) : QStringLiteral("(invalid)"));

  const auto end = index.data(KDGantt::EndTimeRole).toDateTime();
  parts << QStringLiteral("end=%1").arg(end.isValid() ? end.toString(Qt::ISODate)
                                                      : QStringLiteral("(invalid)"));

  const auto completion = index.data(KDGantt::TaskCompletionRole).toInt();
  parts << QStringLiteral("completion=%1%").arg(completion);

  const auto* model = index.model();
  if (model) {
    const auto parent = index.parent();
    parts << QStringLiteral("parent=%1")
                 .arg(parent.isValid() ? QStringLiteral("row=%2").arg(parent.row())
                                       : QStringLiteral("(root)"));

    const int child_count = model->rowCount(index);
    parts << QStringLiteral("children=%1").arg(child_count);
  }

  return parts.join(QStringLiteral(" | "));
}

QStringList DumpModelTree(QAbstractItemModel* model, const QModelIndex& parent, int depth) {
  QStringList result;
  if (model == nullptr) {
    result << QStringLiteral("(null model)");
    return result;
  }

  const int rows = model->rowCount(parent);
  const int cols = model->columnCount(parent);
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      const auto idx = model->index(r, c, parent);
      if (!idx.isValid()) {
        continue;
      }
      const QString indent(depth * 2, QLatin1Char(' '));
      result << indent + DumpGanttRoles(idx);

      const auto child_errors = DumpModelTree(model, idx, depth + 1);
      result.append(child_errors);
    }
  }
  return result;
}

QStringList ValidateModelTree(QAbstractItemModel* model, const QModelIndex& parent,
                              const QString& path) {
  QStringList results;
  if (model == nullptr) {
    results << QStringLiteral("(null model)");
    return results;
  }

  const int rows = model->rowCount(parent);
  for (int r = 0; r < rows; ++r) {
    const auto idx = model->index(r, 0, parent);
    if (!idx.isValid()) {
      results << QStringLiteral("%1 row=%2: invalid index").arg(path).arg(r);
      continue;
    }

    const QString current_path =
        path.isEmpty() ? QString::number(r) : path + QStringLiteral("/") + QString::number(r);
    const auto display = idx.data(Qt::DisplayRole).toString();
    const auto errors = ValidateGanttIndex(idx);
    for (const auto& err : errors) {
      results << QStringLiteral("%1 (\"%2\"): %3").arg(current_path, display, err);
    }

    const auto child_results = ValidateModelTree(model, idx, current_path);
    results.append(child_results);
  }
  return results;
}

namespace {

class SignalProbe final : public QObject {
 public:
  SignalProbe(QObject* parent, SignalCounters* counters) : QObject(parent), counters_(counters) {
    auto* m = qobject_cast<QAbstractItemModel*>(parent);
    if (m == nullptr) {
      return;
    }
    connect(m, &QAbstractItemModel::dataChanged, this, [this]() { ++counters_->dataChanged; });
    connect(m, &QAbstractItemModel::rowsInserted, this, [this]() { ++counters_->rowsInserted; });
    connect(m, &QAbstractItemModel::rowsRemoved, this, [this]() { ++counters_->rowsRemoved; });
    connect(m, &QAbstractItemModel::layoutChanged, this, [this]() { ++counters_->layoutChanged; });
    connect(m, &QAbstractItemModel::modelReset, this, [this]() { ++counters_->modelReset; });
  }

 private:
  SignalCounters* counters_;
};

}  // namespace

void InstallSignalProbe(QAbstractItemModel* model, SignalCounters* counters) {
  if (model == nullptr || counters == nullptr) {
    return;
  }
  new SignalProbe(model, counters);
}

VisibilityResult CheckTaskVisibility(const QModelIndex& index) {
  VisibilityResult result;
  if (!index.isValid()) {
    result.reasons << QStringLiteral("Index is invalid");
    return result;
  }

  const auto type = static_cast<KDGantt::ItemType>(index.data(KDGantt::ItemTypeRole).toInt());
  if (type == KDGantt::TypeNone) {
    result.reasons << QStringLiteral("ItemType is TypeNone");
    return result;
  }

  const auto start = index.data(KDGantt::StartTimeRole).toDateTime();
  if (!start.isValid()) {
    result.reasons << QStringLiteral("StartTime is invalid");
  }

  const auto end = index.data(KDGantt::EndTimeRole).toDateTime();
  if (!end.isValid()) {
    result.reasons << QStringLiteral("EndTime is invalid");
  }

  if (start.isValid() && end.isValid() && end < start) {
    result.reasons << QStringLiteral("EndTime before StartTime");
  }

  if (type == KDGantt::TypeSummary) {
    const auto* model = index.model();
    if (model == nullptr || model->rowCount(index) == 0) {
      result.reasons << QStringLiteral("Summary type but has no children");
    }
  }

  result.visible = result.reasons.isEmpty();
  return result;
}

QString GenerateReport(QAbstractItemModel* model, const QString& label) {
  QStringList lines;
  lines << QStringLiteral("=== Gantt Model Diagnostic Report: %1 ===").arg(label);
  lines << QString();

  if (model == nullptr) {
    lines << QStringLiteral("Model is null.");
    return lines.join(QStringLiteral("\n"));
  }

  const int row_count = model->rowCount();
  const int col_count = model->columnCount();
  lines << QStringLiteral("Rows: %1, Columns: %2").arg(row_count).arg(col_count);

  if (const auto* proxy = qobject_cast<const QAbstractProxyModel*>(model)) {
    lines << QStringLiteral("Type: QAbstractProxyModel (maps from source model)");
    if (const auto* src = proxy->sourceModel()) {
      lines << QStringLiteral("  Source model rows: %1").arg(src->rowCount());
    }
  } else if (qobject_cast<const QStandardItemModel*>(model)) {
    lines << QStringLiteral("Type: QStandardItemModel");
  } else {
    lines << QStringLiteral("Type: %1").arg(QString::fromLatin1(model->metaObject()->className()));
  }

  lines << QString();

  const auto errors = ValidateModelTree(model);
  if (errors.isEmpty()) {
    lines << QStringLiteral("Validation: PASS (no errors)");
  } else {
    lines << QStringLiteral("Validation: %1 error(s)").arg(errors.size());
    for (const auto& err : errors) {
      lines << QStringLiteral("  [ERR] %1").arg(err);
    }
  }

  lines << QString();
  lines << QStringLiteral("Model tree dump:");
  const auto tree = DumpModelTree(model);
  if (tree.isEmpty()) {
    lines << QStringLiteral("  (empty)");
  } else {
    for (const auto& line : tree) {
      lines << QStringLiteral("  %1").arg(line);
    }
  }

  lines << QString();
  lines << QStringLiteral("=== End Report: %1 ===").arg(label);
  return lines.join(QStringLiteral("\n"));
}

}  // namespace cppwiki::gui::project_board::gantt::diagnostic