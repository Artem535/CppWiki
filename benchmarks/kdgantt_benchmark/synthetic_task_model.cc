#include "synthetic_task_model.h"

#include <KDGanttConstraintModel>
#include <KDGanttGlobal>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QStandardItem>
#include <QTimeZone>
#include <QUuid>

namespace cppwiki::gui::project_board::gantt::benchmark {

namespace {

auto MakeId() -> QString {
  return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

auto RandomDate(QRandomGenerator* rng, const QDateTime& base, int daySpan) -> QDateTime {
  const int offset = static_cast<int>(rng->bounded(daySpan));
  return base.addDays(offset);
}

}  // namespace

QJsonObject BuildSyntheticBoard(const DatasetParams& params) {
  QRandomGenerator rng(42);

  const auto base = QDateTime(QDate(2025, 1, 1), QTime(0, 0), QTimeZone::UTC);

  QJsonArray tasks;
  QJsonArray links;

  struct TaskInfo {
    QString id;
    QString parentId;
    int depth = 0;
  };
  QVector<TaskInfo> taskInfos;

  for (int i = 0; i < params.taskCount; ++i) {
    QJsonObject task;
    const auto id = MakeId();
    task.insert(QStringLiteral("id"), id);
    task.insert(QStringLiteral("text"), QStringLiteral("Task %1").arg(i));

    const auto start = RandomDate(&rng, base, params.daySpan);
    const int duration =
        static_cast<int>(rng.bounded(params.maxDurationDays - params.minDurationDays + 1)) +
        params.minDurationDays;
    const auto end = start.addDays(duration);

    task.insert(QStringLiteral("start"), start.toString(Qt::ISODateWithMs));
    task.insert(QStringLiteral("end"), end.toString(Qt::ISODateWithMs));
    task.insert(QStringLiteral("duration"), duration);
    task.insert(QStringLiteral("progress"), static_cast<int>(rng.bounded(101)));

    const double type_roll = rng.generateDouble();
    if (type_roll < params.milestoneRatio) {
      task.insert(QStringLiteral("type"), QStringLiteral("milestone"));
    } else if (type_roll < params.milestoneRatio + params.summaryRatio) {
      task.insert(QStringLiteral("type"), QStringLiteral("summary"));
    } else {
      task.insert(QStringLiteral("type"), QStringLiteral("task"));
    }

    // Determine parent for hierarchy
    int depth = 0;
    QString parentId;
    if (params.maxDepth > 0 && i > 0) {
      const double parent_roll = rng.generateDouble();
      if (parent_roll < 0.3 && !taskInfos.isEmpty()) {
        const auto& candidate = taskInfos[rng.bounded(taskInfos.size())];
        if (candidate.depth < params.maxDepth - 1) {
          parentId = candidate.id;
          depth = candidate.depth + 1;
        }
      }
    }
    if (!parentId.isEmpty()) {
      task.insert(QStringLiteral("parent"), parentId);
    } else {
      depth = 0;
    }

    taskInfos.push_back({id, parentId, depth});
    tasks.push_back(task);
  }

  // Dependencies
  for (int i = 0; i < params.dependencyCount && tasks.size() >= 2; ++i) {
    const auto src = taskInfos[rng.bounded(taskInfos.size())].id;
    const auto dst = taskInfos[rng.bounded(taskInfos.size())].id;
    if (src == dst) {
      continue;
    }

    QJsonObject link;
    link.insert(QStringLiteral("id"), MakeId());
    link.insert(QStringLiteral("source"), src);
    link.insert(QStringLiteral("target"), dst);
    link.insert(QStringLiteral("type"), QStringLiteral("e2s"));
    links.push_back(link);
  }

  QJsonObject board;
  board.insert(QStringLiteral("tasks"), tasks);
  board.insert(QStringLiteral("links"), links);
  board.insert(QStringLiteral("columns"), QJsonArray{});

  return board;
}

void PopulateModel(QStandardItemModel* model, KDGantt::ConstraintModel* constraintModel,
                   const DatasetParams& params) {
  const auto board = BuildSyntheticBoard(params);

  const auto tasks = board.value(QStringLiteral("tasks")).toArray();
  QHash<QString, QStandardItem*> items_by_id;
  QHash<QString, QString> parent_by_id;

  for (const auto& entry : tasks) {
    if (!entry.isObject()) {
      continue;
    }
    const auto task = entry.toObject();
    const auto id = task.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) {
      continue;
    }

    auto* item = new QStandardItem(task.value(QStringLiteral("text")).toString());
    item->setEditable(true);

    const auto start =
        QDateTime::fromString(task.value(QStringLiteral("start")).toString(), Qt::ISODateWithMs);
    const auto end =
        QDateTime::fromString(task.value(QStringLiteral("end")).toString(), Qt::ISODateWithMs);

    const auto type_str = task.value(QStringLiteral("type")).toString();
    KDGantt::ItemType type = KDGantt::TypeTask;
    if (type_str == QStringLiteral("summary")) {
      type = KDGantt::TypeSummary;
    } else if (type_str == QStringLiteral("milestone")) {
      type = KDGantt::TypeEvent;
    }

    item->setData(static_cast<int>(type), KDGantt::ItemTypeRole);
    item->setData(start, KDGantt::StartTimeRole);
    item->setData(end, KDGantt::EndTimeRole);
    item->setData(task.value(QStringLiteral("progress")).toInt(0), KDGantt::TaskCompletionRole);
    item->setData(id, Qt::UserRole + 2000);

    items_by_id.insert(id, item);
    parent_by_id.insert(id, task.value(QStringLiteral("parent")).toString());
  }

  for (const auto& entry : tasks) {
    if (!entry.isObject()) {
      continue;
    }
    const auto id = entry.toObject().value(QStringLiteral("id")).toString();
    auto* item = items_by_id.value(id);
    if (item == nullptr) {
      continue;
    }

    const auto parent_id = parent_by_id.value(id);
    auto* parent_item = parent_id.isEmpty() ? nullptr : items_by_id.value(parent_id);
    if (parent_item != nullptr) {
      parent_item->appendRow(item);
    } else {
      model->appendRow(item);
    }
  }

  if (constraintModel != nullptr) {
    const auto links = board.value(QStringLiteral("links")).toArray();
    for (const auto& entry : links) {
      if (!entry.isObject()) {
        continue;
      }
      const auto link = entry.toObject();
      const auto source_id = link.value(QStringLiteral("source")).toString();
      const auto target_id = link.value(QStringLiteral("target")).toString();
      auto* src_item = items_by_id.value(source_id);
      auto* dst_item = items_by_id.value(target_id);
      if (src_item == nullptr || dst_item == nullptr) {
        continue;
      }
      // Find the model indexes
      const auto source_idx = model->indexFromItem(src_item);
      const auto target_idx = model->indexFromItem(dst_item);
      if (source_idx.isValid() && target_idx.isValid()) {
        KDGantt::Constraint constraint(source_idx, target_idx, KDGantt::Constraint::TypeHard);
        constraintModel->addConstraint(constraint);
      }
    }
  }
}

void InjectTestCase(QStandardItemModel* model, TestCase tc, int row) {
  auto* item = new QStandardItem();
  const auto now = QDateTime::currentDateTimeUtc();

  switch (tc) {
    case TestCase::ValidTask:
      item->setText(QStringLiteral("ValidTask"));
      item->setData(KDGantt::TypeTask, KDGantt::ItemTypeRole);
      item->setData(now, KDGantt::StartTimeRole);
      item->setData(now.addDays(5), KDGantt::EndTimeRole);
      item->setData(50, KDGantt::TaskCompletionRole);
      break;
    case TestCase::MissingStart:
      item->setText(QStringLiteral("MissingStart"));
      item->setData(KDGantt::TypeTask, KDGantt::ItemTypeRole);
      item->setData(now.addDays(5), KDGantt::EndTimeRole);
      item->setData(0, KDGantt::TaskCompletionRole);
      break;
    case TestCase::MissingEnd:
      item->setText(QStringLiteral("MissingEnd"));
      item->setData(KDGantt::TypeTask, KDGantt::ItemTypeRole);
      item->setData(now, KDGantt::StartTimeRole);
      item->setData(0, KDGantt::TaskCompletionRole);
      break;
    case TestCase::ZeroDurationTask:
      item->setText(QStringLiteral("ZeroDurationTask"));
      item->setData(KDGantt::TypeTask, KDGantt::ItemTypeRole);
      item->setData(now, KDGantt::StartTimeRole);
      item->setData(now, KDGantt::EndTimeRole);
      item->setData(0, KDGantt::TaskCompletionRole);
      break;
    case TestCase::EndBeforeStart:
      item->setText(QStringLiteral("EndBeforeStart"));
      item->setData(KDGantt::TypeTask, KDGantt::ItemTypeRole);
      item->setData(now.addDays(5), KDGantt::StartTimeRole);
      item->setData(now, KDGantt::EndTimeRole);
      item->setData(0, KDGantt::TaskCompletionRole);
      break;
    case TestCase::OnePixelTask:
      item->setText(QStringLiteral("OnePixelTask"));
      item->setData(KDGantt::TypeTask, KDGantt::ItemTypeRole);
      item->setData(now, KDGantt::StartTimeRole);
      item->setData(now.addMSecs(1), KDGantt::EndTimeRole);
      item->setData(0, KDGantt::TaskCompletionRole);
      break;
    case TestCase::SummaryNoChildren:
      item->setText(QStringLiteral("SummaryNoChildren"));
      item->setData(KDGantt::TypeSummary, KDGantt::ItemTypeRole);
      item->setData(now, KDGantt::StartTimeRole);
      item->setData(now.addDays(5), KDGantt::EndTimeRole);
      item->setData(0, KDGantt::TaskCompletionRole);
      break;
    case TestCase::TypeMulti:
      item->setText(QStringLiteral("TypeMulti"));
      item->setData(KDGantt::TypeMulti, KDGantt::ItemTypeRole);
      item->setData(now, KDGantt::StartTimeRole);
      item->setData(now.addDays(5), KDGantt::EndTimeRole);
      item->setData(0, KDGantt::TaskCompletionRole);
      break;
    case TestCase::TaskBeforeGrid:
      item->setText(QStringLiteral("TaskBeforeGrid"));
      item->setData(KDGantt::TypeTask, KDGantt::ItemTypeRole);
      item->setData(now.addYears(-10), KDGantt::StartTimeRole);
      item->setData(now.addYears(-10).addDays(5), KDGantt::EndTimeRole);
      item->setData(0, KDGantt::TaskCompletionRole);
      break;
    case TestCase::TaskAfterGrid:
      item->setText(QStringLiteral("TaskAfterGrid"));
      item->setData(KDGantt::TypeTask, KDGantt::ItemTypeRole);
      item->setData(now.addYears(10), KDGantt::StartTimeRole);
      item->setData(now.addYears(10).addDays(5), KDGantt::EndTimeRole);
      item->setData(0, KDGantt::TaskCompletionRole);
      break;
    case TestCase::DstBoundary:
      item->setText(QStringLiteral("DstBoundary"));
      item->setData(KDGantt::TypeTask, KDGantt::ItemTypeRole);
      {
        // Spring forward: March 9, 2025 02:00 → 03:00 in US timezones
        QDateTime dst_start(QDate(2025, 3, 9), QTime(1, 0), QTimeZone::UTC);
        item->setData(dst_start, KDGantt::StartTimeRole);
        item->setData(dst_start.addDays(1), KDGantt::EndTimeRole);
      }
      item->setData(0, KDGantt::TaskCompletionRole);
      break;
  }

  if (row < 0 || row >= model->rowCount()) {
    model->appendRow(item);
  } else {
    model->insertRow(row, item);
  }
}

}  // namespace cppwiki::gui::project_board::gantt::benchmark