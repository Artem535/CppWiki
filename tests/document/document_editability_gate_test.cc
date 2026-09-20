#include "document/document_editability_gate.h"

#include <QString>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "storage/fake_document_repository.h"

namespace {

using cppwiki::document::DocumentEditabilityGate;
using cppwiki::document::EditabilityCheckResult;
using cppwiki::document::IsDocumentConflicted;
using cppwiki::document::LockStatusCheckResult;
using cppwiki::storage::DocumentConflictRecord;
using cppwiki::storage::testing::FakeDocumentRepository;

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

auto MakeGate(std::shared_ptr<FakeDocumentRepository> repository, int* lock_check_calls,
              LockStatusCheckResult lock_result) -> DocumentEditabilityGate {
  return DocumentEditabilityGate(
      repository, [lock_check_calls, lock_result](const QString&,
                                                   std::function<void(LockStatusCheckResult)> on_done) {
        if (lock_check_calls != nullptr) {
          ++*lock_check_calls;
        }
        on_done(lock_result);
      });
}

auto TestConflictedDocumentIsNotEditableAndSkipsTheLockCheck() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  const auto conflict = DocumentConflictRecord{
      .id = "conflict-1", .document_id = "doc-1", .resolution_state = "pending"};
  repository->SaveConflict(conflict);

  int lock_check_calls = 0;
  const auto gate =
      MakeGate(repository, &lock_check_calls, LockStatusCheckResult{.checked_successfully = true});

  std::optional<EditabilityCheckResult> result;
  gate.Check(QStringLiteral("doc-1"), [&result](auto r) { result = r; });

  Require(result.has_value(), "Check must call on_done exactly once");
  Require(!result->editable, "a conflicted document must not be editable");
  Require(result->has_conflict, "the result must report the conflict as the reason");
  Require(lock_check_calls == 0,
          "the lock status checker must not be called once a conflict already rejects the document");
}

auto TestLockedDocumentIsNotEditable() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  int lock_check_calls = 0;
  const auto gate = MakeGate(
      repository, &lock_check_calls,
      LockStatusCheckResult{.checked_successfully = true, .lock_owner = QStringLiteral("alice")});

  std::optional<EditabilityCheckResult> result;
  gate.Check(QStringLiteral("doc-2"), [&result](auto r) { result = r; });

  Require(lock_check_calls == 1, "an unconflicted document must have its lock status checked");
  Require(result.has_value() && !result->editable, "a locked document must not be editable");
  Require(!result->has_conflict, "the rejection reason must be the lock, not a conflict");
  Require(result->lock_owner == QStringLiteral("alice"),
          "the lock owner reported by the checker must be surfaced");
}

auto TestUnlockedUnconflictedDocumentIsEditable() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  const auto gate =
      MakeGate(repository, nullptr, LockStatusCheckResult{.checked_successfully = true});

  std::optional<EditabilityCheckResult> result;
  gate.Check(QStringLiteral("doc-3"), [&result](auto r) { result = r; });

  Require(result.has_value() && result->editable,
          "a document with no conflict and no lock owner must be editable");
}

auto TestAFailedLockCheckFailsClosed() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  const auto gate =
      MakeGate(repository, nullptr, LockStatusCheckResult{.checked_successfully = false});

  std::optional<EditabilityCheckResult> result;
  gate.Check(QStringLiteral("doc-4"), [&result](auto r) { result = r; });

  Require(result.has_value() && !result->editable,
          "a lock status check that could not complete must fail closed, not be treated as unlocked");
}

auto TestResolvedConflictDoesNotBlockEditing() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  repository->SaveConflict(DocumentConflictRecord{
      .id = "conflict-2", .document_id = "doc-5", .resolution_state = "resolved"});
  const auto gate =
      MakeGate(repository, nullptr, LockStatusCheckResult{.checked_successfully = true});

  Require(!IsDocumentConflicted(*repository, QStringLiteral("doc-5")),
          "a resolved conflict must not count as a pending conflict");
}

}  // namespace

auto main() -> int {
  TestConflictedDocumentIsNotEditableAndSkipsTheLockCheck();
  TestLockedDocumentIsNotEditable();
  TestUnlockedUnconflictedDocumentIsEditable();
  TestAFailedLockCheckFailsClosed();
  TestResolvedConflictDoesNotBlockEditing();
  std::cout << "cppwiki_document_editability_gate_tests passed\n";
  return EXIT_SUCCESS;
}
