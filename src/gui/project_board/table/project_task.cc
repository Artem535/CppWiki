#include "gui/project_board/table/project_task.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLatin1String>
#include <QTimeZone>

namespace cppwiki::gui::project_board {

namespace {

constexpr auto kIdKey = "id";
constexpr auto kTextKey = "text";
constexpr auto kStartKey = "start";
constexpr auto kDurationKey = "duration";
constexpr auto kProgressKey = "progress";
constexpr auto kColumnKey = "column";
constexpr auto kPriorityKey = "priority";
constexpr auto kLabelKey = "label";
constexpr auto kTasksKey = "tasks";
constexpr auto kColumnsKey = "columns";
constexpr auto kLinksKey = "links";

// Parses the ISO 8601 date-time string produced by JS's Date.toISOString() (see
// projectBoard.ts's fromParsedTasks()), e.g. "2026-07-24T00:00:00.000Z". Falls back to a
// plain "yyyy-MM-dd" date if the value isn't a full date-time, and returns an invalid QDate for
// anything unparseable or missing.
QDate ParseIsoDate(const QString& value) {
  if (value.isEmpty()) {
    return QDate();
  }
  QDateTime date_time = QDateTime::fromString(value, Qt::ISODateWithMs);
  if (!date_time.isValid()) {
    date_time = QDateTime::fromString(value, Qt::ISODate);
  }
  if (date_time.isValid()) {
    return date_time.date();
  }
  return QDate::fromString(value.left(10), Qt::ISODate);
}

// Formats a QDate as a UTC ISO 8601 date-time string matching what fromParsedTasks() would have
// written for a midnight-UTC Date — keeps this component's serialized output shaped exactly like
// the web version's, rather than merely "parseable by it".
QString FormatIsoDate(const QDate& date) {
  const QDateTime date_time(date, QTime(0, 0, 0), QTimeZone::utc());
  return date_time.toString(Qt::ISODateWithMs);
}

QVector<ProjectColumn> DefaultColumns() {
  return {
      ProjectColumn{QStringLiteral("todo"), QStringLiteral("To do")},
      ProjectColumn{QStringLiteral("inProgress"), QStringLiteral("In progress")},
      ProjectColumn{QStringLiteral("done"), QStringLiteral("Done")},
  };
}

}  // namespace

QString PriorityLabel(int priority) {
  switch (priority) {
    case kPriorityLow:
      return QStringLiteral("Low");
    case kPriorityMedium:
      return QStringLiteral("Medium");
    case kPriorityHigh:
      return QStringLiteral("High");
    default:
      return QString();
  }
}

QString ProjectTask::id() const {
  return json_.value(QLatin1String(kIdKey)).toString();
}

void ProjectTask::setId(const QString& id) {
  json_[QLatin1String(kIdKey)] = id;
}

QString ProjectTask::text() const {
  return json_.value(QLatin1String(kTextKey)).toString();
}

void ProjectTask::setText(const QString& text) {
  json_[QLatin1String(kTextKey)] = text;
}

QDate ProjectTask::start() const {
  return ParseIsoDate(json_.value(QLatin1String(kStartKey)).toString());
}

void ProjectTask::setStart(const QDate& date) {
  json_[QLatin1String(kStartKey)] = FormatIsoDate(date);
}

int ProjectTask::duration() const {
  return json_.value(QLatin1String(kDurationKey)).toInt();
}

void ProjectTask::setDuration(int duration) {
  json_[QLatin1String(kDurationKey)] = duration;
}

int ProjectTask::progress() const {
  return json_.value(QLatin1String(kProgressKey)).toInt();
}

void ProjectTask::setProgress(int progress) {
  json_[QLatin1String(kProgressKey)] = progress;
}

QString ProjectTask::column() const {
  return json_.value(QLatin1String(kColumnKey)).toString();
}

void ProjectTask::setColumn(const QString& column_id) {
  json_[QLatin1String(kColumnKey)] = column_id;
}

bool ProjectTask::hasPriority() const {
  return json_.contains(QLatin1String(kPriorityKey));
}

int ProjectTask::priority() const {
  return json_.value(QLatin1String(kPriorityKey)).toInt();
}

void ProjectTask::setPriority(int priority) {
  json_[QLatin1String(kPriorityKey)] = priority;
}

void ProjectTask::clearPriority() {
  json_.remove(QLatin1String(kPriorityKey));
}

QJsonObject ToJson(const ProjectColumn& column) {
  QJsonObject json;
  json[QLatin1String(kIdKey)] = column.id;
  json[QLatin1String(kLabelKey)] = column.label;
  return json;
}

ProjectColumn ColumnFromJson(const QJsonObject& json) {
  ProjectColumn column;
  column.id = json.value(QLatin1String(kIdKey)).toString();
  column.label = json.value(QLatin1String(kLabelKey)).toString();
  return column;
}

std::optional<ProjectBoardDocument> ParseProjectBoardJson(const QString& raw_content) {
  if (raw_content.trimmed().isEmpty()) {
    ProjectBoardDocument document;
    document.columns = DefaultColumns();
    return document;
  }

  QJsonParseError parse_error{};
  const QJsonDocument json = QJsonDocument::fromJson(raw_content.toUtf8(), &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !json.isObject()) {
    return std::nullopt;
  }

  const QJsonObject object = json.object();
  if (!object.value(QLatin1String(kTasksKey)).isArray() ||
      !object.value(QLatin1String(kColumnsKey)).isArray()) {
    return std::nullopt;
  }

  ProjectBoardDocument document;
  for (const QJsonValue& value : object.value(QLatin1String(kTasksKey)).toArray()) {
    if (value.isObject()) {
      document.tasks.append(ProjectTask(value.toObject()));
    }
  }
  for (const QJsonValue& value : object.value(QLatin1String(kColumnsKey)).toArray()) {
    if (value.isObject()) {
      document.columns.append(ColumnFromJson(value.toObject()));
    }
  }
  if (object.value(QLatin1String(kLinksKey)).isArray()) {
    document.links = object.value(QLatin1String(kLinksKey)).toArray();
  }
  return document;
}

QString SerializeProjectBoardJson(const ProjectBoardDocument& document) {
  QJsonArray tasks_array;
  for (const ProjectTask& task : document.tasks) {
    tasks_array.append(task.toJson());
  }

  QJsonArray columns_array;
  for (const ProjectColumn& column : document.columns) {
    columns_array.append(ToJson(column));
  }

  QJsonObject object;
  object[QLatin1String(kTasksKey)] = tasks_array;
  object[QLatin1String(kColumnsKey)] = columns_array;
  if (!document.links.isEmpty()) {
    object[QLatin1String(kLinksKey)] = document.links;
  }

  return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

}  // namespace cppwiki::gui::project_board
