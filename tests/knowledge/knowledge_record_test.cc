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
  TestRelationRejectsSamePageEndpoints();
  std::cout << "cppwiki_knowledge_record_tests passed\n";
  return EXIT_SUCCESS;
}
