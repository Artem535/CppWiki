#ifndef CPPWIKI_SRC_KNOWLEDGE_KNOWLEDGE_RECORD_H_
#define CPPWIKI_SRC_KNOWLEDGE_KNOWLEDGE_RECORD_H_

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

struct PageRelation {
  std::string id;
  std::string workspace_id;
  std::string relation_type_id;
  std::string source_page_id;
  std::string target_page_id;
  AuditMetadata audit;
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

}  // namespace cppwiki::knowledge

#endif  // CPPWIKI_SRC_KNOWLEDGE_KNOWLEDGE_RECORD_H_
