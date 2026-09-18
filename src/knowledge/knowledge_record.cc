#include "knowledge/knowledge_record.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <string_view>

namespace cppwiki::knowledge {
namespace {

auto IsBlank(std::string_view value) -> bool {
  return std::all_of(value.begin(), value.end(), [](unsigned char character) {
    return std::isspace(character) != 0;
  });
}

auto HasAuditMetadata(const AuditMetadata& audit) -> bool {
  return !audit.created_at.empty() && !audit.updated_at.empty() && !audit.created_by.empty() &&
         !audit.updated_by.empty();
}

auto HasDuplicateOrBlankValues(const std::vector<std::string>& values) -> bool {
  for (const auto& value : values) {
    if (value.empty() || IsBlank(value)) {
      return true;
    }
  }
  for (auto first = values.begin(); first != values.end(); ++first) {
    if (std::find(std::next(first), values.end(), *first) != values.end()) {
      return true;
    }
  }
  return false;
}

auto HasOption(const PropertyDefinition& definition, std::string_view value) -> bool {
  return std::ranges::find(definition.options, value) != definition.options.end();
}

auto IsSingleValueKind(PropertyValueKind kind) -> bool {
  return kind == PropertyValueKind::kText || kind == PropertyValueKind::kNumber ||
         kind == PropertyValueKind::kDate || kind == PropertyValueKind::kCheckbox ||
         kind == PropertyValueKind::kSelect;
}

}  // namespace

auto ValidatePropertyDefinition(const PropertyDefinition& definition) -> std::optional<std::string> {
  if (definition.id.empty() || definition.workspace_id.empty()) {
    return "Property definition id and workspace id must be non-empty.";
  }
  if (definition.name.empty() || IsBlank(definition.name)) {
    return "Property definition name must be non-empty.";
  }
  if (definition.group_name && (definition.group_name->empty() || IsBlank(*definition.group_name))) {
    return "Property definition group name must not be blank.";
  }
  if (!HasAuditMetadata(definition.audit)) {
    return "Property definition audit metadata must be complete.";
  }

  const bool needs_options = definition.value_kind == PropertyValueKind::kSelect ||
                             definition.value_kind == PropertyValueKind::kMultiSelect;
  if (needs_options && definition.options.empty()) {
    return "Select property definitions must define options.";
  }
  if (!needs_options && !definition.options.empty()) {
    return "Only select property definitions may define options.";
  }
  if (HasDuplicateOrBlankValues(definition.options)) {
    return "Property definition options must be unique and non-blank.";
  }
  return std::nullopt;
}

auto ValidatePagePropertyValue(const PagePropertyValue& value,
                               const PropertyDefinition& definition)
    -> std::optional<std::string> {
  if (const auto definition_error = ValidatePropertyDefinition(definition)) {
    return "Property definition is invalid: " + *definition_error;
  }
  if (value.id.empty() || value.workspace_id.empty() || value.page_id.empty() ||
      value.property_definition_id.empty()) {
    return "Page property value identity and scope must be non-empty.";
  }
  if (value.workspace_id != definition.workspace_id ||
      value.property_definition_id != definition.id) {
    return "Page property value must refer to its property definition in the same workspace.";
  }
  if (!HasAuditMetadata(value.audit)) {
    return "Page property value audit metadata must be complete.";
  }
  if (value.values.empty() || HasDuplicateOrBlankValues(value.values)) {
    return "Page property values must be non-empty, unique and non-blank.";
  }
  if (IsSingleValueKind(definition.value_kind) && value.values.size() != 1) {
    return "This property value kind requires exactly one value.";
  }
  if (definition.value_kind == PropertyValueKind::kCheckbox &&
      value.values.front() != "true" && value.values.front() != "false") {
    return "Checkbox property values must be true or false.";
  }
  if (definition.value_kind == PropertyValueKind::kNumber) {
    double number{};
    const auto [end, error] =
        std::from_chars(value.values.front().data(),
                        value.values.front().data() + value.values.front().size(), number);
    if (error != std::errc() || end != value.values.front().data() + value.values.front().size()) {
      return "Number property values must contain a number.";
    }
  }
  if (definition.value_kind == PropertyValueKind::kSelect ||
      definition.value_kind == PropertyValueKind::kMultiSelect) {
    for (const auto& selected : value.values) {
      if (!HasOption(definition, selected)) {
        return "Select property values must be declared by their definition.";
      }
    }
  }
  return std::nullopt;
}

auto ValidateRelationType(const RelationType& relation_type) -> std::optional<std::string> {
  if (relation_type.id.empty() || relation_type.workspace_id.empty()) {
    return "Relation type id and workspace id must be non-empty.";
  }
  if (relation_type.name.empty() || IsBlank(relation_type.name)) {
    return "Relation type name must be non-empty.";
  }
  if (!HasAuditMetadata(relation_type.audit)) {
    return "Relation type audit metadata must be complete.";
  }
  if (relation_type.direction == RelationDirection::kDirected &&
      (!relation_type.inverse_name || relation_type.inverse_name->empty() ||
       IsBlank(*relation_type.inverse_name))) {
    return "Directed relation types require a non-blank inverse name.";
  }
  if (relation_type.direction == RelationDirection::kSymmetric && relation_type.inverse_name) {
    return "Symmetric relation types must not define an inverse name.";
  }
  return std::nullopt;
}

auto NormalizeAndValidatePageRelation(PageRelation* relation, const RelationType& relation_type)
    -> std::optional<std::string> {
  if (relation == nullptr) {
    return "Page relation must not be null.";
  }
  if (const auto type_error = ValidateRelationType(relation_type)) {
    return "Relation type is invalid: " + *type_error;
  }
  if (relation->id.empty() || relation->workspace_id.empty() || relation->relation_type_id.empty() ||
      relation->source_page_id.empty() || relation->target_page_id.empty()) {
    return "Page relation identity, scope and endpoints must be non-empty.";
  }
  if (relation->workspace_id != relation_type.workspace_id ||
      relation->relation_type_id != relation_type.id) {
    return "Page relation must refer to its relation type in the same workspace.";
  }
  if (relation->source_kind != ArtifactKind::kPage || relation->target_kind != ArtifactKind::kPage) {
    return "Page relation only supports page endpoints until non-page artifact kinds exist.";
  }
  if (!HasAuditMetadata(relation->audit)) {
    return "Page relation audit metadata must be complete.";
  }
  if (relation->source_page_id == relation->target_page_id) {
    return "Page relation endpoints must be distinct.";
  }
  if (relation_type.direction == RelationDirection::kSymmetric &&
      relation->target_page_id < relation->source_page_id) {
    std::swap(relation->source_page_id, relation->target_page_id);
  }
  return std::nullopt;
}

auto ValidateRepositoryArtifact(const RepositoryArtifact& repository) -> std::optional<std::string> {
  if (repository.id.empty() || repository.workspace_id.empty()) {
    return "Repository artifact id and workspace id must be non-empty.";
  }
  if (repository.name.empty() || IsBlank(repository.name)) {
    return "Repository artifact name must be non-empty.";
  }
  if (repository.remote_url.empty() || IsBlank(repository.remote_url)) {
    return "Repository artifact remote url must be non-empty.";
  }
  if (repository.default_branch.empty() || IsBlank(repository.default_branch)) {
    return "Repository artifact default branch must be non-empty.";
  }
  if (!HasAuditMetadata(repository.audit)) {
    return "Repository artifact audit metadata must be complete.";
  }
  return std::nullopt;
}

namespace {

auto IsTerminalAgentRunStatus(AgentRunStatus status) -> bool {
  return status == AgentRunStatus::kSucceeded || status == AgentRunStatus::kFailed ||
         status == AgentRunStatus::kCancelled;
}

}  // namespace

auto ValidateAgentRun(const AgentRun& run) -> std::optional<std::string> {
  if (run.id.empty() || run.workspace_id.empty() || run.task_id.empty()) {
    return "Agent run identity and scope must be non-empty.";
  }
  if (run.context_pack_ref.empty() || IsBlank(run.context_pack_ref)) {
    return "Agent run must reference a context pack.";
  }
  if (run.runtime_id.empty() || IsBlank(run.runtime_id)) {
    return "Agent run must name its runtime.";
  }
  if (run.started_at.empty()) {
    return "Agent run must record when it started.";
  }
  if (!HasAuditMetadata(run.audit)) {
    return "Agent run audit metadata must be complete.";
  }
  const bool is_terminal = IsTerminalAgentRunStatus(run.status);
  if (is_terminal && !run.completed_at) {
    return "A terminal agent run must record when it completed.";
  }
  if (!is_terminal && run.completed_at) {
    return "A non-terminal agent run must not record a completion time.";
  }
  return std::nullopt;
}

auto ValidateAgentRunTransition(AgentRunStatus from, AgentRunStatus to) -> std::optional<std::string> {
  if (IsTerminalAgentRunStatus(from)) {
    return "A terminal agent run status can never transition.";
  }
  // Explicit edges, not a level/ordinal comparison: pending may only reach running or cancelled
  // (a run can be cancelled before it starts), and succeeded/failed are reachable only through
  // running — a run must actually run before it can succeed or fail.
  if (from == AgentRunStatus::kPending) {
    if (to == AgentRunStatus::kRunning || to == AgentRunStatus::kCancelled) {
      return std::nullopt;
    }
    return "A pending agent run may only move to running or cancelled.";
  }
  if (from == AgentRunStatus::kRunning) {
    if (IsTerminalAgentRunStatus(to)) {
      return std::nullopt;
    }
    return "A running agent run may only move to a terminal status.";
  }
  return "Unsupported agent run status transition.";
}

auto ValidateResultReference(const ResultReference& reference, const AgentRun& run)
    -> std::optional<std::string> {
  if (const auto run_error = ValidateAgentRun(run)) {
    return "Agent run is invalid: " + *run_error;
  }
  if (reference.id.empty() || reference.workspace_id.empty() || reference.agent_run_id.empty()) {
    return "Result reference identity and scope must be non-empty.";
  }
  if (reference.workspace_id != run.workspace_id || reference.agent_run_id != run.id) {
    return "Result reference must refer to its agent run in the same workspace.";
  }
  if (reference.locator.empty() || IsBlank(reference.locator)) {
    return "Result reference must have a non-blank locator.";
  }
  if (reference.created_at.empty() || reference.created_by.empty()) {
    return "Result reference must record when and by whom it was created.";
  }
  return std::nullopt;
}

auto ValidateContextPack(const ContextPack& pack) -> std::optional<std::string> {
  if (pack.id.empty() || pack.workspace_id.empty() || pack.task_id.empty()) {
    return "Context pack identity and scope must be non-empty.";
  }
  if (pack.task_intent.empty() || IsBlank(pack.task_intent)) {
    return "Context pack must capture a non-blank task intent.";
  }
  if (pack.repository_guidance &&
      (pack.repository_guidance->empty() || IsBlank(*pack.repository_guidance))) {
    return "Context pack repository guidance must not be blank.";
  }
  if (!HasAuditMetadata(pack.audit)) {
    return "Context pack audit metadata must be complete.";
  }
  if (pack.items.empty()) {
    return "Context pack must capture at least one selected artifact.";
  }

  bool has_included_item = false;
  for (const auto& item : pack.items) {
    if (item.id.empty() || item.relation_id.empty() || item.artifact_id.empty()) {
      return "Context pack item identity, provenance and artifact id must be non-empty.";
    }
    has_included_item = has_included_item || item.included;
  }
  for (auto first = pack.items.begin(); first != pack.items.end(); ++first) {
    const auto duplicate = std::find_if(
        std::next(first), pack.items.end(),
        [first](const ContextPackItem& other) { return other.relation_id == first->relation_id; });
    if (duplicate != pack.items.end()) {
      return "Context pack items must each come from a distinct source relation.";
    }
  }

  if (pack.state == ContextPackState::kApproved && !has_included_item) {
    return "An approved context pack must have at least one included item.";
  }
  return std::nullopt;
}

}  // namespace cppwiki::knowledge
