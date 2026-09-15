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

}  // namespace cppwiki::knowledge
