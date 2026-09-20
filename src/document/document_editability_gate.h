#ifndef CPPWIKI_SRC_DOCUMENT_DOCUMENT_EDITABILITY_GATE_H_
#define CPPWIKI_SRC_DOCUMENT_DOCUMENT_EDITABILITY_GATE_H_

#include <functional>
#include <memory>

#include <QString>

#include "storage/local_document_repository.h"

namespace cppwiki::document {

// Whether an arbitrary document is currently locked, per one lock-status query. Distinct
// from `checked_successfully = false` (the query itself failed, e.g. the backend is
// unreachable) versus `lock_owner` empty (query succeeded, nobody holds the lock).
struct LockStatusCheckResult {
  bool checked_successfully = false;
  QString lock_owner;
};

// The outcome of DocumentEditabilityGate::Check().
struct EditabilityCheckResult {
  bool editable = true;
  bool has_conflict = false;
  QString lock_owner;
};

// True when `document_id` has a conflict record whose resolution_state is still "pending",
// per the same rule Page::ApplyConflictStateForDocument (src/gui/page.cc) already applies
// for the currently-open document -- shared here so the two call sites cannot drift.
[[nodiscard]] auto IsDocumentConflicted(storage::LocalDocumentRepository& repository,
                                        const QString& document_id) -> bool;

// Whether an arbitrary document (by id) may be mutated right now, independent of whatever
// document happens to be open in Documents mode -- see ADR-020. Unlike
// QEditorBridge::RejectIfCurrentDocumentLocked/Conflicted (ADR-010, ADR-013), which only
// ever check the currently-open document, this takes the target id directly.
class DocumentEditabilityGate final {
 public:
  // Performs the actual (network) lock lookup for a document id and reports the result via
  // its own callback. The caller supplies this (see BackendClient::CheckDocumentLockStatus)
  // so this class does not depend on backend::BackendClient directly.
  using LockStatusChecker =
      std::function<void(const QString& document_id,
                         std::function<void(LockStatusCheckResult)>)>;

  DocumentEditabilityGate(std::shared_ptr<storage::LocalDocumentRepository> repository,
                          LockStatusChecker lock_status_checker);

  // Checks conflict state synchronously first (a document already known to be conflicted
  // never needs a network round trip); `on_done` is invoked exactly once with the combined
  // result. A lock-status query that fails is treated as not editable (fail closed).
  void Check(const QString& document_id, std::function<void(EditabilityCheckResult)> on_done) const;

 private:
  std::shared_ptr<storage::LocalDocumentRepository> repository_;
  LockStatusChecker lock_status_checker_;
};

}  // namespace cppwiki::document

#endif  // CPPWIKI_SRC_DOCUMENT_DOCUMENT_EDITABILITY_GATE_H_
