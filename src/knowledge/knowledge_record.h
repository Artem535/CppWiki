#ifndef CPPWIKI_SRC_KNOWLEDGE_KNOWLEDGE_RECORD_H_
#define CPPWIKI_SRC_KNOWLEDGE_KNOWLEDGE_RECORD_H_

#include <cstdint>

#include <optional>
#include <string>
#include <vector>

namespace cppwiki::knowledge {

enum class PropertyValueKind {
  kText,
  kNumber,
  kDate,
  kCheckbox,
  kSelect,
  kMultiSelect,
  kTags,
  kRelation,
};

enum class RecordState { kActive, kRetired };

struct AuditMetadata {
  std::string created_at;
  std::string updated_at;
  std::string created_by;
  std::string updated_by;
};

struct PropertyDefinition {
  std::string id;
  std::string workspace_id;
  std::string name;
  std::optional<std::string> group_name;
  PropertyValueKind value_kind{PropertyValueKind::kText};
  std::vector<std::string> options;
  RecordState state{RecordState::kActive};
  AuditMetadata audit;
};

struct PagePropertyValue {
  std::string id;
  std::string workspace_id;
  std::string page_id;
  std::string property_definition_id;
  std::vector<std::string> values;
  AuditMetadata audit;
};

enum class RelationDirection { kDirected, kSymmetric };

struct RelationType {
  std::string id;
  std::string workspace_id;
  std::string name;
  std::optional<std::string> inverse_name;
  RelationDirection direction{RelationDirection::kDirected};
  RecordState state{RecordState::kActive};
  AuditMetadata audit;
};

// Engineering Context Artifact Model Contract (#201, ADR-019): the discriminator that
// generalizes PageRelation into ArtifactRelation. kPage is the default so every relation record
// written before this field existed round-trips unchanged. kRepository/kAgentRun/kResultReference
// exist in the schema ahead of their record types landing, so the wire shape is stable once they
// do; NormalizeAndValidatePageRelation() rejects them until then (see knowledge_record.cc).
enum class ArtifactKind : std::uint8_t { kPage, kRepository, kAgentRun, kResultReference };

struct PageRelation {
  std::string id;
  std::string workspace_id;
  std::string relation_type_id;
  std::string source_page_id;
  std::string target_page_id;
  ArtifactKind source_kind{ArtifactKind::kPage};
  ArtifactKind target_kind{ArtifactKind::kPage};
  AuditMetadata audit;
};

// Engineering Context Artifact Model Contract (#201): a reference to a git repository a Task can
// target — this repo itself, or any external repository a configured agent runtime is pointed
// at. `state` reuses RecordState the same way PropertyDefinition/RelationType do: kActive means
// available for new work, kRetired ("archived" in the contract's prose) means history stays
// readable but no new work may target it.
struct RepositoryArtifact {
  std::string id;
  std::string workspace_id;
  std::string name;
  std::string remote_url;
  std::string default_branch;
  RecordState state{RecordState::kActive};
  AuditMetadata audit;
};

// Forward-moving execution state machine: pending -> running -> {succeeded, failed, cancelled}.
// Terminal states (succeeded/failed/cancelled) are final — see ValidateAgentRunTransition().
enum class AgentRunStatus : std::uint8_t { kPending, kRunning, kSucceeded, kFailed, kCancelled };

// The execution record linking a Task to one attempt by an external agent runtime.
// `context_pack_ref` is treated as an opaque, immutable token here — its internal shape belongs
// to the Context Pack contract (#202), not this record.
struct AgentRun {
  std::string id;
  std::string workspace_id;
  std::string task_id;
  std::string context_pack_ref;
  std::string runtime_id;
  AgentRunStatus status{AgentRunStatus::kPending};
  std::string started_at;
  std::optional<std::string> completed_at;
  AuditMetadata audit;
};

// A pointer to where an AgentRun's output actually lives, external to CppWiki — never the
// content itself, only the locator (ADR-015's trust-boundary principle, applied here too).
enum class ResultReferenceKind : std::uint8_t { kGitRef, kDiff, kLog, kExternalUrl };

struct ResultReference {
  std::string id;
  std::string workspace_id;
  std::string agent_run_id;
  ResultReferenceKind kind{ResultReferenceKind::kGitRef};
  std::string locator;
  std::optional<std::string> summary;
  std::string created_at;
  std::string created_by;
};

[[nodiscard]] auto ValidatePropertyDefinition(const PropertyDefinition& definition)
    -> std::optional<std::string>;
[[nodiscard]] auto ValidatePagePropertyValue(const PagePropertyValue& value,
                                             const PropertyDefinition& definition)
    -> std::optional<std::string>;
[[nodiscard]] auto ValidateRelationType(const RelationType& relation_type)
    -> std::optional<std::string>;
[[nodiscard]] auto NormalizeAndValidatePageRelation(PageRelation* relation,
                                                     const RelationType& relation_type)
    -> std::optional<std::string>;
[[nodiscard]] auto ValidateRepositoryArtifact(const RepositoryArtifact& repository)
    -> std::optional<std::string>;
[[nodiscard]] auto ValidateAgentRun(const AgentRun& run) -> std::optional<std::string>;
// Pure state-machine legality check, independent of any single AgentRun record — a caller
// persisting a status change validates the transition before applying it.
[[nodiscard]] auto ValidateAgentRunTransition(AgentRunStatus from, AgentRunStatus to)
    -> std::optional<std::string>;
[[nodiscard]] auto ValidateResultReference(const ResultReference& reference, const AgentRun& run)
    -> std::optional<std::string>;

}  // namespace cppwiki::knowledge

#endif  // CPPWIKI_SRC_KNOWLEDGE_KNOWLEDGE_RECORD_H_
