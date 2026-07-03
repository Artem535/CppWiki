#ifndef CPPWIKI_SRC_GUI_MERGE_CONFLICT_MERGE_RESOLUTION_H_
#define CPPWIKI_SRC_GUI_MERGE_CONFLICT_MERGE_RESOLUTION_H_

#include <QJsonObject>
#include <QString>

#include "storage/local_document_repository.h"

namespace cppwiki::gui::merge {

enum class MergeResolutionStatus {
  kApplied,
  kConflictUnavailable,
  kInvalidMergedSnapshot,
  kSaveFailed,
  kResolveFailed,
};

struct MergeResolutionResult {
  MergeResolutionStatus status{MergeResolutionStatus::kInvalidMergedSnapshot};
  QString message;
};

enum class ConflictResolutionSide {
  kLocal,
  kRemote,
};

[[nodiscard]] auto ApplyMergedConflictResolution(
    storage::LocalDocumentRepository& repository, const QString& conflict_id,
    const QString& author_id, const QJsonObject& merged_document) -> MergeResolutionResult;

[[nodiscard]] auto ApplyConflictSideResolution(storage::LocalDocumentRepository& repository,
                                               const QString& conflict_id,
                                               ConflictResolutionSide side)
    -> MergeResolutionResult;

}  // namespace cppwiki::gui::merge

#endif  // CPPWIKI_SRC_GUI_MERGE_CONFLICT_MERGE_RESOLUTION_H_
