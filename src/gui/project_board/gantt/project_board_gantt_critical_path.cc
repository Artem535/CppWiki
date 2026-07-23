#include "gui/project_board/gantt/project_board_gantt_critical_path.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonValue>
#include <QVector>
#include <limits>

namespace cppwiki::gui::project_board::gantt {

namespace {

enum class RelationType { kStartStart, kStartFinish, kFinishFinish, kFinishStart };

auto RelationTypeFromString(const QString& type) -> RelationType {
  if (type == QStringLiteral("s2s")) {
    return RelationType::kStartStart;
  }
  if (type == QStringLiteral("s2e")) {
    return RelationType::kStartFinish;
  }
  if (type == QStringLiteral("e2e")) {
    return RelationType::kFinishFinish;
  }
  return RelationType::kFinishStart;  // "e2s", and the default per the shared schema.
}

struct Edge {
  QString source;
  QString target;
  RelationType type;
  double lag;
  QString link_id;
};

constexpr double kEpsilon = 1e-6;

}  // namespace

auto ComputeCriticalPath(const QJsonObject& board) -> CriticalPathResult {
  CriticalPathResult result;

  const auto tasks = board.value(QStringLiteral("tasks")).toArray();
  QHash<QString, double> duration_by_id;
  QVector<QString> ids;
  for (const auto& entry : tasks) {
    if (!entry.isObject()) {
      continue;
    }
    const auto task = entry.toObject();
    const auto id = task.value(QStringLiteral("id")).toString();
    if (id.isEmpty() || duration_by_id.contains(id)) {
      continue;
    }
    duration_by_id.insert(id, qMax(0, task.value(QStringLiteral("duration")).toInt(1)));
    ids.push_back(id);
  }
  if (ids.isEmpty()) {
    return result;
  }

  const auto links = board.value(QStringLiteral("links")).toArray();
  QVector<Edge> edges;
  QHash<QString, QVector<int>> outgoing;
  QHash<QString, QVector<int>> incoming;
  QHash<QString, int> indegree;
  for (const auto& id : ids) {
    indegree.insert(id, 0);
  }
  for (const auto& entry : links) {
    if (!entry.isObject()) {
      continue;
    }
    const auto link = entry.toObject();
    const auto source = link.value(QStringLiteral("source")).toString();
    const auto target = link.value(QStringLiteral("target")).toString();
    if (source.isEmpty() || target.isEmpty() || source == target ||
        !duration_by_id.contains(source) || !duration_by_id.contains(target)) {
      continue;
    }
    Edge edge;
    edge.source = source;
    edge.target = target;
    edge.type = RelationTypeFromString(link.value(QStringLiteral("type")).toString());
    edge.lag = link.value(QStringLiteral("lag")).toDouble(0.0);
    edge.link_id = link.value(QStringLiteral("id")).toString();
    const int edge_index = static_cast<int>(edges.size());
    edges.push_back(edge);
    outgoing[source].push_back(edge_index);
    incoming[target].push_back(edge_index);
    indegree[target] += 1;
  }

  // Kahn's algorithm: topologically sort the task ids so both passes below can process every
  // task strictly after (forward pass) or before (backward pass) everything it depends on.
  QVector<QString> topo_order;
  QVector<QString> queue;
  for (const auto& id : ids) {
    if (indegree.value(id) == 0) {
      queue.push_back(id);
    }
  }
  QHash<QString, int> remaining_indegree = indegree;
  while (!queue.isEmpty()) {
    const auto id = queue.takeLast();
    topo_order.push_back(id);
    for (int edge_index : outgoing.value(id)) {
      const auto& target = edges[edge_index].target;
      remaining_indegree[target] -= 1;
      if (remaining_indegree[target] == 0) {
        queue.push_back(target);
      }
    }
  }
  if (topo_order.size() != ids.size()) {
    // Cycle: CPM is undefined on this graph. Report nothing rather than a misleading highlight.
    return result;
  }

  // Forward pass: earliest start/finish for every task, in topological order.
  QHash<QString, double> earliest_start;
  QHash<QString, double> earliest_finish;
  for (const auto& id : topo_order) {
    double start = 0.0;
    for (int edge_index : incoming.value(id)) {
      const auto& edge = edges[edge_index];
      const double duration = duration_by_id.value(id);
      double required = 0.0;
      switch (edge.type) {
        case RelationType::kFinishStart:
          required = earliest_finish.value(edge.source) + edge.lag;
          break;
        case RelationType::kStartStart:
          required = earliest_start.value(edge.source) + edge.lag;
          break;
        case RelationType::kFinishFinish:
          required = earliest_finish.value(edge.source) + edge.lag - duration;
          break;
        case RelationType::kStartFinish:
          required = earliest_start.value(edge.source) + edge.lag - duration;
          break;
      }
      start = qMax(start, required);
    }
    earliest_start.insert(id, start);
    earliest_finish.insert(id, start + duration_by_id.value(id));
  }

  double project_end = 0.0;
  for (const auto& id : ids) {
    project_end = qMax(project_end, earliest_finish.value(id));
  }

  // Backward pass: latest start/finish, in reverse topological order.
  QHash<QString, double> latest_start;
  QHash<QString, double> latest_finish;
  for (auto it = topo_order.crbegin(); it != topo_order.crend(); ++it) {
    const auto& id = *it;
    double finish = project_end;
    for (int edge_index : outgoing.value(id)) {
      const auto& edge = edges[edge_index];
      const double duration = duration_by_id.value(id);
      double allowed = std::numeric_limits<double>::max();
      switch (edge.type) {
        case RelationType::kFinishStart:
          allowed = latest_start.value(edge.target) - edge.lag;
          break;
        case RelationType::kStartStart:
          allowed = latest_start.value(edge.target) - edge.lag + duration;
          break;
        case RelationType::kFinishFinish:
          allowed = latest_finish.value(edge.target) - edge.lag;
          break;
        case RelationType::kStartFinish:
          allowed = latest_finish.value(edge.target) - edge.lag + duration;
          break;
      }
      finish = qMin(finish, allowed);
    }
    latest_finish.insert(id, finish);
    latest_start.insert(id, finish - duration_by_id.value(id));
  }

  for (const auto& id : ids) {
    if (qAbs(latest_start.value(id) - earliest_start.value(id)) < kEpsilon) {
      result.critical_task_ids.insert(id);
    }
  }
  for (const auto& edge : edges) {
    if (result.critical_task_ids.contains(edge.source) &&
        result.critical_task_ids.contains(edge.target) && !edge.link_id.isEmpty()) {
      result.critical_link_ids.insert(edge.link_id);
    }
  }

  return result;
}

}  // namespace cppwiki::gui::project_board::gantt
