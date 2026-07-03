#include "gui/merge/conflict_merge_resolution.h"

#include <QDateTime>
#include <QByteArray>
#include <QJsonDocument>
#include <QProcessEnvironment>

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

#include "document/document_validator.h"

namespace cppwiki::gui::merge {

namespace {

auto CurrentAuthorIdFallback() -> std::string {
  const auto environment = QProcessEnvironment::systemEnvironment();
  const auto user = environment.value(QStringLiteral("USER"));
  return user.isEmpty() ? std::string("merge-editor") : user.toStdString();
}

auto EffectiveAuthorId(const QString& author_id) -> std::string {
  const auto trimmed = author_id.trimmed();
  return trimmed.isEmpty() ? CurrentAuthorIdFallback() : trimmed.toStdString();
}

auto NormalizedConflictAuthor(const std::string& author) -> std::string {
  return author.empty() ? std::string("conflict-resolution") : author;
}

auto LoadCurrentDocument(storage::LocalDocumentRepository& repository,
                         std::string_view document_id)
    -> std::optional<storage::DocumentRecord> {
  const auto loaded = repository.LoadDocument(document_id);
  if (loaded.error || !loaded.document.has_value()) {
    return std::nullopt;
  }
  return loaded.document;
}

auto ApplyConflictSnapshotResolution(storage::LocalDocumentRepository& repository,
                                     const QString& conflict_id,
                                     const storage::DocumentConflictRecord& conflict,
                                     std::string author, const QByteArray& snapshot_bytes,
                                     QString invalid_snapshot_message,
                                     QString save_failed_prefix,
                                     QString applied_message) -> MergeResolutionResult {
  const auto validation = document::DocumentValidator::ParseAndValidateSnapshot(snapshot_bytes);
  if (validation.error || !validation.document.has_value() || !validation.snapshot.has_value()) {
    return {
        .status = MergeResolutionStatus::kInvalidMergedSnapshot,
        .message = std::move(invalid_snapshot_message),
    };
  }

  const auto current_document = LoadCurrentDocument(repository, conflict.document_id);
  storage::DocumentRecord record;
  if (current_document.has_value()) {
    record = *current_document;
  }

  const auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();

  record.metadata.id = conflict.document_id;
  record.metadata.workspace_id = conflict.workspace_id;
  record.metadata.title = validation.document->metadata.title;
  record.metadata.schema_version = document::SchemaVersion::kV1;
  record.metadata.updated_at = now;
  record.metadata.updated_by = author;
  if (record.metadata.created_at.empty()) {
    record.metadata.created_at = now;
  }
  if (record.metadata.created_by.empty()) {
    record.metadata.created_by = author;
  }
  record.metadata.content_version =
      std::max<std::int64_t>(std::max<std::int64_t>(record.metadata.content_version, 1),
                             conflict.base_version) +
      1;
  record.snapshot = *validation.snapshot;
  record.raw_snapshot_json = validation.raw_snapshot_json;

  const auto save_result = repository.SaveDocument(record);
  if (save_result.error) {
    return {
        .status = MergeResolutionStatus::kSaveFailed,
        .message = QStringLiteral("%1: %2").arg(
            save_failed_prefix, QString::fromStdString(save_result.error->message)),
    };
  }

  const auto resolve_result = repository.ResolveConflict(conflict_id.toStdString());
  if (resolve_result.error) {
    return {
        .status = MergeResolutionStatus::kResolveFailed,
        .message = QStringLiteral("Save succeeded, but conflict could not be resolved."),
    };
  }

  return {
      .status = MergeResolutionStatus::kApplied,
      .message = std::move(applied_message),
  };
}

}  // namespace

auto ApplyMergedConflictResolution(storage::LocalDocumentRepository& repository,
                                   const QString& conflict_id, const QString& author_id,
                                   const QJsonObject& merged_document)
    -> MergeResolutionResult {
  const auto conflict = repository.LoadConflict(conflict_id.toStdString());
  if (conflict.error || !conflict.conflict.has_value()) {
    return {
        .status = MergeResolutionStatus::kConflictUnavailable,
        .message = QStringLiteral("Cannot save: conflict record is unavailable."),
    };
  }

  const auto merged_bytes = QJsonDocument(merged_document).toJson(QJsonDocument::Compact);
  return ApplyConflictSnapshotResolution(
      repository, conflict_id, *conflict.conflict, EffectiveAuthorId(author_id), merged_bytes,
      QStringLiteral("Cannot save: merged snapshot is invalid."),
      QStringLiteral("Save failed"), QStringLiteral("Merge saved and conflict resolved."));
}

auto ApplyConflictSideResolution(storage::LocalDocumentRepository& repository,
                                 const QString& conflict_id, ConflictResolutionSide side)
    -> MergeResolutionResult {
  const auto conflict = repository.LoadConflict(conflict_id.toStdString());
  if (conflict.error || !conflict.conflict.has_value()) {
    return {
        .status = MergeResolutionStatus::kConflictUnavailable,
        .message = QStringLiteral("Conflict load failed: %1")
                       .arg(conflict.error
                                ? QString::fromStdString(conflict.error->message)
                                : QStringLiteral("Conflict was not found.")),
    };
  }

  const auto use_remote = side == ConflictResolutionSide::kRemote;
  const auto& snapshot =
      use_remote ? conflict.conflict->remote_snapshot : conflict.conflict->local_snapshot;
  const auto author = NormalizedConflictAuthor(
      use_remote ? conflict.conflict->remote_updated_by : conflict.conflict->local_updated_by);
  const auto action = use_remote ? QStringLiteral("Use remote") : QStringLiteral("Use local");
  const auto applied_message =
      use_remote ? QStringLiteral("Applied remote version and resolved conflict.")
                 : QStringLiteral("Applied local version and resolved conflict.");

  return ApplyConflictSnapshotResolution(
      repository, conflict_id, *conflict.conflict, author, QByteArray::fromStdString(snapshot),
      QStringLiteral("%1 failed: Conflict snapshot could not be parsed.").arg(action),
      QStringLiteral("%1 failed").arg(action), applied_message);
}

}  // namespace cppwiki::gui::merge
