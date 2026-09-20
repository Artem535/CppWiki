#include "knowledge/knowledge_record.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

auto MakeAudit() -> cppwiki::knowledge::AuditMetadata {
  return {
      .created_at = "2026-09-08T10:00:00.000Z",
      .updated_at = "2026-09-08T10:00:00.000Z",
      .created_by = "tester",
      .updated_by = "tester",
  };
}

auto MakeSelectDefinition() -> cppwiki::knowledge::PropertyDefinition {
  return {
      .id = "property-status",
      .workspace_id = "engineering",
      .name = "Status",
      .group_name = std::nullopt,
      .value_kind = cppwiki::knowledge::PropertyValueKind::kSelect,
      .options = {"Draft", "Approved"},
      .audit = MakeAudit(),
  };
}

auto TestPropertyDefinitionRequiresStableIdentityAndName() -> void {
  auto definition = MakeSelectDefinition();
  definition.id.clear();
  Require(cppwiki::knowledge::ValidatePropertyDefinition(definition).has_value(),
          "a property definition without an id must be invalid");

  definition = MakeSelectDefinition();
  definition.workspace_id.clear();
  Require(cppwiki::knowledge::ValidatePropertyDefinition(definition).has_value(),
          "a property definition without a workspace must be invalid");

  definition = MakeSelectDefinition();
  definition.name = "  ";
  Require(cppwiki::knowledge::ValidatePropertyDefinition(definition).has_value(),
          "a property definition without a name must be invalid");
}

auto TestSelectValueMustUseDefinitionOption() -> void {
  const auto definition = MakeSelectDefinition();
  const cppwiki::knowledge::PagePropertyValue value{
      .id = "value-status",
      .workspace_id = "engineering",
      .page_id = "page-auth",
      .property_definition_id = "property-status",
      .values = {"Deprecated"},
      .audit = MakeAudit(),
  };
  Require(cppwiki::knowledge::ValidatePagePropertyValue(value, definition).has_value(),
          "a select value outside its definition options must be invalid");
}

auto TestDirectedRelationRequiresInverseName() -> void {
  const cppwiki::knowledge::RelationType relation_type{
      .id = "relation-uses",
      .workspace_id = "engineering",
      .name = "uses",
      .inverse_name = std::nullopt,
      .direction = cppwiki::knowledge::RelationDirection::kDirected,
      .audit = MakeAudit(),
  };
  Require(cppwiki::knowledge::ValidateRelationType(relation_type).has_value(),
          "a directed relation type without an inverse name must be invalid");
}

auto TestSymmetricRelationNormalizesEndpointOrder() -> void {
  const cppwiki::knowledge::RelationType relation_type{
      .id = "relation-related",
      .workspace_id = "engineering",
      .name = "related to",
      .inverse_name = std::nullopt,
      .direction = cppwiki::knowledge::RelationDirection::kSymmetric,
      .audit = MakeAudit(),
  };
  cppwiki::knowledge::PageRelation relation{
      .id = "edge-1",
      .workspace_id = "engineering",
      .relation_type_id = "relation-related",
      .source_page_id = "page-z",
      .target_page_id = "page-a",
      .audit = MakeAudit(),
  };
  Require(!cppwiki::knowledge::NormalizeAndValidatePageRelation(&relation, relation_type),
          "a symmetric relation with two workspace pages must be valid");
  Require(relation.source_page_id == "page-a" && relation.target_page_id == "page-z",
          "a symmetric relation must use a stable endpoint order");
}

// Engineering Context Artifact Model Contract (#201, ADR-019): PageRelation gains optional
// source_kind/target_kind fields generalizing it into ArtifactRelation. Absence still means
// "page" (existing rows/callers are unaffected — see TestSymmetricRelationNormalizesEndpointOrder
// above, which sets neither field), and an explicit "page" on both ends behaves identically.
auto TestRelationAcceptsExplicitPageEndpointKinds() -> void {
  const cppwiki::knowledge::RelationType relation_type{
      .id = "relation-related",
      .workspace_id = "engineering",
      .name = "related to",
      .inverse_name = std::nullopt,
      .direction = cppwiki::knowledge::RelationDirection::kSymmetric,
      .audit = MakeAudit(),
  };
  cppwiki::knowledge::PageRelation relation{
      .id = "edge-1",
      .workspace_id = "engineering",
      .relation_type_id = "relation-related",
      .source_page_id = "page-a",
      .target_page_id = "page-b",
      .source_kind = cppwiki::knowledge::ArtifactKind::kPage,
      .target_kind = cppwiki::knowledge::ArtifactKind::kPage,
      .audit = MakeAudit(),
  };
  Require(!cppwiki::knowledge::NormalizeAndValidatePageRelation(&relation, relation_type),
          "a relation with explicit page/page endpoint kinds must be valid");
}

// Engineering Context Artifact Model Contract, "context for" (#201/#203): a relation from a
// Task (page) to a RepositoryArtifact -- the first non-page endpoint kind PageRelation actually
// needs to persist. The directed "context for"/"context provided by" type is used here rather
// than the symmetric fixture above since that's the real relation type this endpoint shape
// exists for.
auto TestRelationAcceptsARepositoryTargetEndpoint() -> void {
  const cppwiki::knowledge::RelationType relation_type{
      .id = "relation-context-for",
      .workspace_id = "engineering",
      .name = "context for",
      .inverse_name = "context provided by",
      .direction = cppwiki::knowledge::RelationDirection::kDirected,
      .audit = MakeAudit(),
  };
  cppwiki::knowledge::PageRelation relation{
      .id = "edge-1",
      .workspace_id = "engineering",
      .relation_type_id = "relation-context-for",
      .source_page_id = "page-task-a",
      .target_kind = cppwiki::knowledge::ArtifactKind::kRepository,
      .target_id = "repo-cppwiki",
      .audit = MakeAudit(),
  };
  Require(!cppwiki::knowledge::NormalizeAndValidatePageRelation(&relation, relation_type),
          "a task-to-repository-artifact relation must be valid");
}

auto TestRelationRejectsANonPageEndpointMissingItsId() -> void {
  const cppwiki::knowledge::RelationType relation_type{
      .id = "relation-context-for",
      .workspace_id = "engineering",
      .name = "context for",
      .inverse_name = "context provided by",
      .direction = cppwiki::knowledge::RelationDirection::kDirected,
      .audit = MakeAudit(),
  };
  cppwiki::knowledge::PageRelation relation{
      .id = "edge-1",
      .workspace_id = "engineering",
      .relation_type_id = "relation-context-for",
      .source_page_id = "page-task-a",
      .target_kind = cppwiki::knowledge::ArtifactKind::kRepository,
      .audit = MakeAudit(),
  };
  Require(cppwiki::knowledge::NormalizeAndValidatePageRelation(&relation, relation_type)
              .has_value(),
          "a non-page endpoint without its generic id must be invalid");
}

auto TestRelationRejectsAnEndpointSettingBothIdFields() -> void {
  const cppwiki::knowledge::RelationType relation_type{
      .id = "relation-context-for",
      .workspace_id = "engineering",
      .name = "context for",
      .inverse_name = "context provided by",
      .direction = cppwiki::knowledge::RelationDirection::kDirected,
      .audit = MakeAudit(),
  };
  cppwiki::knowledge::PageRelation relation{
      .id = "edge-1",
      .workspace_id = "engineering",
      .relation_type_id = "relation-context-for",
      .source_page_id = "page-task-a",
      .target_page_id = "page-b",
      .target_kind = cppwiki::knowledge::ArtifactKind::kRepository,
      .target_id = "repo-cppwiki",
      .audit = MakeAudit(),
  };
  Require(cppwiki::knowledge::NormalizeAndValidatePageRelation(&relation, relation_type)
              .has_value(),
          "an endpoint carrying both a page id and a generic id must be invalid");
}

auto TestRelationRejectsAPageEndpointCarryingAGenericId() -> void {
  const cppwiki::knowledge::RelationType relation_type{
      .id = "relation-context-for",
      .workspace_id = "engineering",
      .name = "context for",
      .inverse_name = "context provided by",
      .direction = cppwiki::knowledge::RelationDirection::kDirected,
      .audit = MakeAudit(),
  };
  cppwiki::knowledge::PageRelation relation{
      .id = "edge-1",
      .workspace_id = "engineering",
      .relation_type_id = "relation-context-for",
      .source_page_id = "page-task-a",
      .target_kind = cppwiki::knowledge::ArtifactKind::kRepository,
      .source_id = "unexpected",
      .target_id = "repo-cppwiki",
      .audit = MakeAudit(),
  };
  Require(cppwiki::knowledge::NormalizeAndValidatePageRelation(&relation, relation_type)
              .has_value(),
          "a page endpoint carrying a generic id alongside its page id must be invalid");
}

auto MakeRepositoryArtifact() -> cppwiki::knowledge::RepositoryArtifact {
  return {
      .id = "repo-cppwiki",
      .workspace_id = "engineering",
      .name = "CppWiki",
      .remote_url = "git@github.com:Artem535/CppWiki.git",
      .default_branch = "main",
      .state = cppwiki::knowledge::RecordState::kActive,
      .audit = MakeAudit(),
  };
}

auto TestRepositoryArtifactRequiresIdentityAndRemoteUrl() -> void {
  auto repository = MakeRepositoryArtifact();
  repository.id.clear();
  Require(cppwiki::knowledge::ValidateRepositoryArtifact(repository).has_value(),
          "a repository artifact without an id must be invalid");

  repository = MakeRepositoryArtifact();
  repository.remote_url = "  ";
  Require(cppwiki::knowledge::ValidateRepositoryArtifact(repository).has_value(),
          "a repository artifact without a remote url must be invalid");

  repository = MakeRepositoryArtifact();
  Require(!cppwiki::knowledge::ValidateRepositoryArtifact(repository),
          "a well-formed repository artifact must be valid");
}

auto MakeAgentRun() -> cppwiki::knowledge::AgentRun {
  return {
      .id = "run-a",
      .workspace_id = "engineering",
      .task_id = "page-task-a",
      .context_pack_ref = "pack-v1",
      .runtime_id = "claude-code",
      .status = cppwiki::knowledge::AgentRunStatus::kRunning,
      .started_at = "2026-09-16T00:00:00.000Z",
      .completed_at = std::nullopt,
      .audit = MakeAudit(),
  };
}

auto TestAgentRunRequiresCompletedAtOnlyWhenTerminal() -> void {
  auto run = MakeAgentRun();
  Require(!cppwiki::knowledge::ValidateAgentRun(run), "a running agent run needs no completed_at");

  run.status = cppwiki::knowledge::AgentRunStatus::kSucceeded;
  Require(cppwiki::knowledge::ValidateAgentRun(run).has_value(),
          "a terminal agent run without completed_at must be invalid");

  run.completed_at = "2026-09-16T01:00:00.000Z";
  Require(!cppwiki::knowledge::ValidateAgentRun(run),
          "a terminal agent run with completed_at must be valid");

  run.status = cppwiki::knowledge::AgentRunStatus::kRunning;
  Require(cppwiki::knowledge::ValidateAgentRun(run).has_value(),
          "a non-terminal agent run must not carry a completed_at");
}

auto TestAgentRunRejectsBackwardStatusTransition() -> void {
  Require(cppwiki::knowledge::ValidateAgentRunTransition(
              cppwiki::knowledge::AgentRunStatus::kSucceeded,
              cppwiki::knowledge::AgentRunStatus::kRunning)
              .has_value(),
          "a terminal agent run must never move back to running");
  Require(cppwiki::knowledge::ValidateAgentRunTransition(
              cppwiki::knowledge::AgentRunStatus::kFailed,
              cppwiki::knowledge::AgentRunStatus::kFailed)
              .has_value(),
          "a terminal agent run must never re-transition, even to the same status");
  Require(cppwiki::knowledge::ValidateAgentRunTransition(
              cppwiki::knowledge::AgentRunStatus::kPending,
              cppwiki::knowledge::AgentRunStatus::kRunning)
              .has_value() == false,
          "pending to running is a valid forward transition");
  Require(cppwiki::knowledge::ValidateAgentRunTransition(
              cppwiki::knowledge::AgentRunStatus::kPending,
              cppwiki::knowledge::AgentRunStatus::kCancelled)
              .has_value() == false,
          "pending to cancelled is valid (cancelling before the run starts)");
}

// A run must actually run before it can succeed or fail — succeeded/failed are reachable only
// through running, never directly from pending.
auto TestAgentRunRejectsSkippingRunningToReachATerminalStatus() -> void {
  Require(cppwiki::knowledge::ValidateAgentRunTransition(
              cppwiki::knowledge::AgentRunStatus::kPending,
              cppwiki::knowledge::AgentRunStatus::kSucceeded)
              .has_value(),
          "pending must not jump straight to succeeded");
  Require(cppwiki::knowledge::ValidateAgentRunTransition(
              cppwiki::knowledge::AgentRunStatus::kPending,
              cppwiki::knowledge::AgentRunStatus::kFailed)
              .has_value(),
          "pending must not jump straight to failed");
}

auto TestAgentRunAcceptsEveryRunningToTerminalTransition() -> void {
  Require(!cppwiki::knowledge::ValidateAgentRunTransition(
              cppwiki::knowledge::AgentRunStatus::kRunning,
              cppwiki::knowledge::AgentRunStatus::kSucceeded),
          "running to succeeded must be valid");
  Require(!cppwiki::knowledge::ValidateAgentRunTransition(
              cppwiki::knowledge::AgentRunStatus::kRunning,
              cppwiki::knowledge::AgentRunStatus::kFailed),
          "running to failed must be valid");
  Require(!cppwiki::knowledge::ValidateAgentRunTransition(
              cppwiki::knowledge::AgentRunStatus::kRunning,
              cppwiki::knowledge::AgentRunStatus::kCancelled),
          "running to cancelled must be valid");
}

auto TestResultReferenceValidatesAgainstItsAgentRun() -> void {
  const auto run = MakeAgentRun();
  cppwiki::knowledge::ResultReference reference{
      .id = "result-1",
      .workspace_id = "engineering",
      .agent_run_id = "run-a",
      .kind = cppwiki::knowledge::ResultReferenceKind::kGitRef,
      .locator = "refs/heads/agent/run-a-result",
      .summary = std::nullopt,
      .created_at = "2026-09-16T01:00:00.000Z",
      .created_by = "system",
  };
  Require(!cppwiki::knowledge::ValidateResultReference(reference, run),
          "a result reference matching its agent run must be valid");

  reference.agent_run_id = "run-other";
  Require(cppwiki::knowledge::ValidateResultReference(reference, run).has_value(),
          "a result reference must refer to the agent run it was validated against");
}

auto MakeContextPack() -> cppwiki::knowledge::ContextPack {
  return {
      .id = "pack-a",
      .workspace_id = "engineering",
      .task_id = "page-task-a",
      .task_intent = "Fix the login bug",
      .items =
          {
              cppwiki::knowledge::ContextPackItem{
                  .id = "item-1",
                  .relation_id = "edge-1",
                  .artifact_kind = cppwiki::knowledge::ArtifactKind::kPage,
                  .artifact_id = "page-auth",
                  .included = true,
              },
          },
      .repository_guidance = "Follow the existing auth module conventions.",
      .state = cppwiki::knowledge::ContextPackState::kDraft,
      .audit = MakeAudit(),
  };
}

auto TestContextPackRequiresIdentityScopeAndIntent() -> void {
  auto pack = MakeContextPack();
  pack.id.clear();
  Require(cppwiki::knowledge::ValidateContextPack(pack).has_value(),
          "a context pack without an id must be invalid");

  pack = MakeContextPack();
  pack.task_id.clear();
  Require(cppwiki::knowledge::ValidateContextPack(pack).has_value(),
          "a context pack without a task id must be invalid");

  pack = MakeContextPack();
  pack.task_intent = "   ";
  Require(cppwiki::knowledge::ValidateContextPack(pack).has_value(),
          "a context pack without a task intent must be invalid");

  pack = MakeContextPack();
  Require(!cppwiki::knowledge::ValidateContextPack(pack),
          "a well-formed context pack must be valid");
}

auto TestContextPackRequiresAtLeastOneItem() -> void {
  auto pack = MakeContextPack();
  pack.items.clear();
  Require(cppwiki::knowledge::ValidateContextPack(pack).has_value(),
          "a context pack without any selected artifact must be invalid");
}

auto TestContextPackItemRequiresProvenanceAndArtifactIdentity() -> void {
  auto pack = MakeContextPack();
  pack.items.front().relation_id.clear();
  Require(cppwiki::knowledge::ValidateContextPack(pack).has_value(),
          "a context pack item without its source relation must be invalid");

  pack = MakeContextPack();
  pack.items.front().artifact_id.clear();
  Require(cppwiki::knowledge::ValidateContextPack(pack).has_value(),
          "a context pack item without an artifact id must be invalid");
}

// Two items pointing at the same relation would double-count one piece of provenance -- each
// item must come from a distinct ArtifactRelation.
auto TestContextPackRejectsDuplicateItemProvenance() -> void {
  auto pack = MakeContextPack();
  auto second_item = pack.items.front();
  second_item.id = "item-2";
  second_item.artifact_id = "page-other";
  pack.items.push_back(second_item);
  Require(cppwiki::knowledge::ValidateContextPack(pack).has_value(),
          "two context pack items sharing the same source relation must be invalid");
}

// Approving a pack with every item excluded would freeze an empty context -- at least one item
// must remain included for the pack to be approved.
auto TestApprovedContextPackRequiresAnIncludedItem() -> void {
  auto pack = MakeContextPack();
  pack.state = cppwiki::knowledge::ContextPackState::kApproved;
  pack.items.front().included = false;
  Require(cppwiki::knowledge::ValidateContextPack(pack).has_value(),
          "an approved context pack with every item excluded must be invalid");

  pack.items.front().included = true;
  Require(!cppwiki::knowledge::ValidateContextPack(pack),
          "an approved context pack with at least one included item must be valid");
}

auto TestRelationRejectsSamePageEndpoints() -> void {
  const cppwiki::knowledge::RelationType relation_type{
      .id = "relation-related",
      .workspace_id = "engineering",
      .name = "related to",
      .inverse_name = std::nullopt,
      .direction = cppwiki::knowledge::RelationDirection::kSymmetric,
      .audit = MakeAudit(),
  };
  cppwiki::knowledge::PageRelation relation{
      .id = "edge-1",
      .workspace_id = "engineering",
      .relation_type_id = "relation-related",
      .source_page_id = "page-auth",
      .target_page_id = "page-auth",
      .audit = MakeAudit(),
  };
  Require(cppwiki::knowledge::NormalizeAndValidatePageRelation(&relation, relation_type)
              .has_value(),
          "a relation must not relate a page to itself");
}

}  // namespace

auto main() -> int {
  TestPropertyDefinitionRequiresStableIdentityAndName();
  TestSelectValueMustUseDefinitionOption();
  TestDirectedRelationRequiresInverseName();
  TestSymmetricRelationNormalizesEndpointOrder();
  TestRelationAcceptsExplicitPageEndpointKinds();
  TestRelationAcceptsARepositoryTargetEndpoint();
  TestRelationRejectsANonPageEndpointMissingItsId();
  TestRelationRejectsAnEndpointSettingBothIdFields();
  TestRelationRejectsAPageEndpointCarryingAGenericId();
  TestRepositoryArtifactRequiresIdentityAndRemoteUrl();
  TestAgentRunRequiresCompletedAtOnlyWhenTerminal();
  TestAgentRunRejectsBackwardStatusTransition();
  TestAgentRunRejectsSkippingRunningToReachATerminalStatus();
  TestAgentRunAcceptsEveryRunningToTerminalTransition();
  TestResultReferenceValidatesAgainstItsAgentRun();
  TestContextPackRequiresIdentityScopeAndIntent();
  TestContextPackRequiresAtLeastOneItem();
  TestContextPackItemRequiresProvenanceAndArtifactIdentity();
  TestContextPackRejectsDuplicateItemProvenance();
  TestApprovedContextPackRequiresAnIncludedItem();
  TestRelationRejectsSamePageEndpoints();
  std::cout << "cppwiki_knowledge_record_tests passed\n";
  return EXIT_SUCCESS;
}
