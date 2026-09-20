#include "document/document_editability_gate.h"

#include <utility>

namespace cppwiki::document {

auto IsDocumentConflicted(storage::LocalDocumentRepository& repository,
                          const QString& document_id) -> bool {
  const auto listed = repository.ListConflicts();
  if (listed.error) {
    return false;
  }
  for (const auto& conflict : listed.conflicts) {
    if (conflict.resolution_state == "pending" &&
        QString::fromStdString(conflict.document_id) == document_id) {
      return true;
    }
  }
  return false;
}

DocumentEditabilityGate::DocumentEditabilityGate(
    std::shared_ptr<storage::LocalDocumentRepository> repository,
    LockStatusChecker lock_status_checker)
    : repository_(std::move(repository)), lock_status_checker_(std::move(lock_status_checker)) {}

void DocumentEditabilityGate::Check(const QString& document_id,
                                    std::function<void(EditabilityCheckResult)> on_done) const {
  if (IsDocumentConflicted(*repository_, document_id)) {
    on_done(EditabilityCheckResult{.editable = false, .has_conflict = true, .lock_owner = {}});
    return;
  }

  lock_status_checker_(
      document_id, [on_done = std::move(on_done)](LockStatusCheckResult lock_result) {
        const bool editable =
            lock_result.checked_successfully && lock_result.lock_owner.trimmed().isEmpty();
        on_done(EditabilityCheckResult{
            .editable = editable,
            .has_conflict = false,
            .lock_owner = lock_result.checked_successfully ? lock_result.lock_owner : QString(),
        });
      });
}

}  // namespace cppwiki::document
