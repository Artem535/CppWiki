#include "storage/workspace_archive.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include "knowledge/knowledge_record.h"
#include "storage/file_document_repository.h"

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto MakeDocument(std::string id, std::string workspace_id) -> cppwiki::storage::DocumentRecord {
  return cppwiki::storage::DocumentRecord{
      .metadata =
          cppwiki::document::PageMetadata{
              .id = id,
              .schema_version = cppwiki::document::SchemaVersion::kV1,
              .title = "Archived Page " + id,
              .workspace_id = std::move(workspace_id),
              .sort_order = 1,
              .created_at = "2026-06-30T10:00:00.000Z",
              .updated_at = "2026-06-30T10:05:00.000Z",
              .created_by = "creator",
              .updated_by = "editor",
              .content_version = 2,
          },
      .snapshot =
          cppwiki::document::BlockNoteDocumentSnapshot{
              .id = id,
              .schema_version = 1,
              .title = "Archived Page " + id,
              .blocks = {},
          },
      .raw_snapshot_json =
          R"({"id":")" + id + R"(","schema_version":1,"title":"Archived","blocks":[]})",
  };
}

auto TestExportImportRoundTripsDocumentsAndConflicts() -> void {
  const auto source_dir =
      std::filesystem::temp_directory_path() / "cppwiki-workspace-archive-test-source";
  const auto dest_dir =
      std::filesystem::temp_directory_path() / "cppwiki-workspace-archive-test-dest";
  const auto archive_path =
      std::filesystem::temp_directory_path() / "cppwiki-workspace-archive-test.json";
  std::filesystem::remove_all(source_dir);
  std::filesystem::remove_all(dest_dir);
  std::filesystem::remove(archive_path);

  cppwiki::storage::FileDocumentRepository source_repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = source_dir});

  // FileDocumentRepository doesn't implement workspace root records at all (only the CBLite
  // backend does) -- SaveWorkspaceRoot/LoadWorkspaceRoot fall through to the base class's
  // kUnsupported default here. Export/import must still work fine without one; it's an optional
  // capability, exercised separately wherever a backend actually supports it.
  const auto root_save = source_repository.SaveWorkspaceRoot(cppwiki::storage::WorkspaceRootRecord{
      .workspace_id = "engineering",
      .title = "Engineering",
      .created_at = "2026-06-30T09:00:00.000Z",
      .schema_version = 1,
  });
  Require(root_save.error.has_value() &&
              root_save.error->code == cppwiki::storage::RepositoryErrorCode::kUnsupported,
          "FileDocumentRepository is expected not to support workspace root records");

  Require(!source_repository.SaveDocument(MakeDocument("page-1", "engineering")).error,
          "saving page-1 should succeed");
  Require(!source_repository.SaveDocument(MakeDocument("page-2", "engineering")).error,
          "saving page-2 should succeed");
  // A document in a different workspace must not leak into the "engineering" export.
  Require(!source_repository.SaveDocument(MakeDocument("other-page", "design")).error,
          "saving other-page should succeed");
  const auto MakeAttachment = [](std::string id, std::string workspace_id,
                                 std::vector<std::uint8_t> bytes) {
    return cppwiki::storage::AttachmentData{
        .metadata =
            cppwiki::storage::AttachmentMetadata{
                .id = std::move(id),
                .workspace_id = std::move(workspace_id),
                .filename = "architecture.png",
                .mime_type = "image/png",
                .size_bytes = bytes.size(),
                .sha256 = "0123456789abcdef",
                .created_at = "2026-08-31T10:00:00Z",
                .created_by = "tester",
            },
        .bytes = std::move(bytes),
    };
  };
  Require(
      !source_repository.SaveAttachment(MakeAttachment("attachment-1", "engineering", {1, 2, 3, 4}))
           .error,
      "engineering attachment should save");
  Require(!source_repository.SaveAttachment(MakeAttachment("attachment-other", "design", {5, 6, 7}))
               .error,
          "other workspace attachment should save");

  Require(!source_repository
               .SaveConflict(cppwiki::storage::DocumentConflictRecord{
                   .id = "conflict-1",
                   .document_id = "page-1",
                   .workspace_id = "engineering",
                   .base_version = 2,
                   .local_snapshot = R"({"title":"Local"})",
                   .remote_snapshot = R"({"title":"Remote"})",
                   .local_updated_by = "alice",
                   .remote_updated_by = "bob",
                   .detected_at = "2026-06-30T10:06:00.000Z",
                   .resolution_state = "pending",
               })
               .error,
          "saving a conflict should succeed");

  const auto export_result = cppwiki::storage::ExportWorkspaceToFile(
      source_repository, "engineering", archive_path.string());
  Require(!export_result.error, "exporting the workspace should succeed");
  Require(std::filesystem::exists(archive_path), "the archive file should have been written");

  cppwiki::storage::FileDocumentRepository dest_repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = dest_dir});

  const auto import_result =
      cppwiki::storage::ImportWorkspaceFromFile(dest_repository, archive_path.string());
  Require(!import_result.error, "importing the workspace archive should succeed");
  Require(import_result.workspace_id.has_value() && *import_result.workspace_id == "engineering",
          "import should report the restored workspace id");

  const auto restored_page_1 = dest_repository.LoadDocument("page-1");
  Require(restored_page_1.document.has_value(), "page-1 should be restored");
  Require(restored_page_1.document->metadata.title == "Archived Page page-1",
          "page-1's title should round-trip");
  const auto restored_page_2 = dest_repository.LoadDocument("page-2");
  Require(restored_page_2.document.has_value(), "page-2 should be restored");

  const auto restored_other_page = dest_repository.LoadDocument("other-page");
  Require(!restored_other_page.document.has_value(),
          "a document from a different workspace must not be restored");

  const auto restored_conflicts = dest_repository.ListConflicts();
  Require(!restored_conflicts.error, "listing restored conflicts should succeed");
  Require(restored_conflicts.conflicts.size() == 1,
          "exactly the one engineering-workspace conflict should be restored");
  Require(restored_conflicts.conflicts.front().id == "conflict-1",
          "the restored conflict should preserve its id");
  const auto restored_attachment = dest_repository.LoadAttachment("attachment-1", "engineering");
  Require(!restored_attachment.error && restored_attachment.attachment.has_value(),
          "engineering attachment should be restored");
  Require(restored_attachment.attachment->bytes == std::vector<std::uint8_t>({1, 2, 3, 4}),
          "attachment bytes should round-trip");
  Require(!dest_repository.LoadAttachment("attachment-other", "design").attachment.has_value(),
          "other workspace attachment must not be restored");

  std::filesystem::remove_all(source_dir);
  std::filesystem::remove_all(dest_dir);
  std::filesystem::remove(archive_path);
}

auto MakeAudit() -> cppwiki::knowledge::AuditMetadata {
  return {
      .created_at = "2026-09-08T10:00:00.000Z",
      .updated_at = "2026-09-08T10:00:00.000Z",
      .created_by = "tester",
      .updated_by = "tester",
  };
}

// Issue #185: workspace archive now carries the four knowledge record types (property
// definitions, page property values, relation types, page relations) alongside documents and
// conflicts. Export must collect the knowledge records that belong to the exported workspace;
// import must restore them, preserve their stable IDs when they don't collide with anything
// already in the target repository, validate workspace scope, and reject an archive that
// contains dangling references (a value pointing at an unknown property definition, or a
// relation pointing at an unknown relation type or a page that isn't in the archive).
auto TestExportImportRoundTripsKnowledgeRecords() -> void {
  const auto source_dir =
      std::filesystem::temp_directory_path() / "cppwiki-workspace-archive-knowledge-source";
  const auto dest_dir =
      std::filesystem::temp_directory_path() / "cppwiki-workspace-archive-knowledge-dest";
  const auto archive_path =
      std::filesystem::temp_directory_path() / "cppwiki-workspace-archive-knowledge.json";
  std::filesystem::remove_all(source_dir);
  std::filesystem::remove_all(dest_dir);
  std::filesystem::remove(archive_path);

  cppwiki::storage::FileDocumentRepository source_repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = source_dir});

  // Two pages so a relation between them can be persisted.
  Require(!source_repository.SaveDocument(MakeDocument("page-1", "engineering")).error,
          "saving page-1 should succeed");
  Require(!source_repository.SaveDocument(MakeDocument("page-2", "engineering")).error,
          "saving page-2 should succeed");
  // Knowledge in another workspace must not leak into the "engineering" export.
  Require(!source_repository.SaveDocument(MakeDocument("other-page", "design")).error,
          "saving other-page should succeed");

  const auto audit = MakeAudit();
  Require(!source_repository
              .SavePropertyDefinition(cppwiki::knowledge::PropertyDefinition{
                  .id = "property-status",
                  .workspace_id = "engineering",
                  .name = "Status",
                  .group_name = std::nullopt,
                  .value_kind = cppwiki::knowledge::PropertyValueKind::kSelect,
                  .options = {"Draft", "Approved"},
                  .state = cppwiki::knowledge::RecordState::kActive,
                  .audit = audit,
              })
              .error,
          "saving a property definition should succeed");
  Require(!source_repository
              .SavePagePropertyValue(cppwiki::knowledge::PagePropertyValue{
                  .id = "value-page-1-status",
                  .workspace_id = "engineering",
                  .page_id = "page-1",
                  .property_definition_id = "property-status",
                  .values = {"Draft"},
                  .audit = audit,
              })
              .error,
          "saving a page property value should succeed");
  Require(!source_repository
              .SaveRelationType(cppwiki::knowledge::RelationType{
                  .id = "relation-depends-on",
                  .workspace_id = "engineering",
                  .name = "Depends on",
                  .inverse_name = "Required by",
                  .direction = cppwiki::knowledge::RelationDirection::kDirected,
                  .state = cppwiki::knowledge::RecordState::kActive,
                  .audit = audit,
              })
              .error,
          "saving a relation type should succeed");
  Require(!source_repository
              .SavePageRelation(cppwiki::knowledge::PageRelation{
                  .id = "relation-page-1-page-2",
                  .workspace_id = "engineering",
                  .relation_type_id = "relation-depends-on",
                  .source_page_id = "page-1",
                  .target_page_id = "page-2",
                  .audit = audit,
              })
              .error,
          "saving a page relation should succeed");

  const auto export_result = cppwiki::storage::ExportWorkspaceToFile(
      source_repository, "engineering", archive_path.string());
  Require(!export_result.error, "exporting the workspace should succeed");
  Require(std::filesystem::exists(archive_path), "the archive file should have been written");

  cppwiki::storage::FileDocumentRepository dest_repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = dest_dir});

  const auto import_result =
      cppwiki::storage::ImportWorkspaceFromFile(dest_repository, archive_path.string());
  Require(!import_result.error, "importing the workspace archive should succeed");

  const auto definitions = dest_repository.ListPropertyDefinitions("engineering");
  Require(!definitions.error, "listing restored property definitions should succeed");
  Require(definitions.definitions.size() == 1,
          "the property definition should be restored");
  Require(definitions.definitions.front().id == "property-status",
          "the restored property definition should preserve its id");

  const auto values = dest_repository.ListPagePropertyValues("engineering", "page-1");
  Require(!values.error, "listing restored property values should succeed");
  Require(values.values.size() == 1 && values.values.front().id == "value-page-1-status",
          "the restored property value should preserve its id");
  Require(values.values.front().values.front() == "Draft",
          "the restored property value should preserve its value");

  const auto relation_types = dest_repository.ListRelationTypes("engineering");
  Require(!relation_types.error, "listing restored relation types should succeed");
  Require(relation_types.relation_types.size() == 1 &&
              relation_types.relation_types.front().id == "relation-depends-on",
          "the restored relation type should preserve its id");

  const auto relations = dest_repository.ListPageRelations("engineering", "page-1");
  Require(!relations.error, "listing restored relations should succeed");
  Require(relations.relations.size() == 1 &&
              relations.relations.front().id == "relation-page-1-page-2",
          "the restored relation should preserve its id");
  // A knowledge record from a different workspace must not be restored.
  Require(dest_repository.ListPropertyDefinitions("design").definitions.empty(),
          "a property definition from another workspace must not be restored");

  std::filesystem::remove_all(source_dir);
  std::filesystem::remove_all(dest_dir);
  std::filesystem::remove(archive_path);
}

// Import must reject an archive that contains a dangling knowledge reference -- here a page
// property value pointing at a property definition that is not part of the archive and does not
// exist in the target repository. To exercise the real archive format we export a valid
// workspace and then break the reference in the produced JSON, so the dangling link is formed in
// exactly the form ExportWorkspaceToFile writes it. Import must fail rather than silently
// persisting an orphaned value.
auto TestImportRejectsDanglingKnowledgeReference() -> void {
  const auto source_dir =
      std::filesystem::temp_directory_path() / "cppwiki-workspace-archive-knowledge-dangling-src";
  const auto dest_dir =
      std::filesystem::temp_directory_path() / "cppwiki-workspace-archive-knowledge-dangling-dst";
  const auto archive_path =
      std::filesystem::temp_directory_path() / "cppwiki-workspace-archive-knowledge-dangling.json";
  std::filesystem::remove_all(source_dir);
  std::filesystem::remove_all(dest_dir);
  std::filesystem::remove(archive_path);

  {
    cppwiki::storage::FileDocumentRepository source_repository(
        cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = source_dir});
    Require(!source_repository.SaveDocument(MakeDocument("page-1", "engineering")).error,
            "saving page-1 should succeed");
    const auto audit = MakeAudit();
    Require(!source_repository
                .SavePropertyDefinition(cppwiki::knowledge::PropertyDefinition{
                    .id = "property-status",
                    .workspace_id = "engineering",
                    .name = "Status",
                    .value_kind = cppwiki::knowledge::PropertyValueKind::kSelect,
                    .options = {"Draft", "Approved"},
                    .state = cppwiki::knowledge::RecordState::kActive,
                    .audit = audit,
                })
                .error,
            "saving a property definition should succeed");
    Require(!source_repository
                .SavePagePropertyValue(cppwiki::knowledge::PagePropertyValue{
                    .id = "value-page-1-status",
                    .workspace_id = "engineering",
                    .page_id = "page-1",
                    .property_definition_id = "property-status",
                    .values = {"Draft"},
                    .audit = audit,
                })
                .error,
            "saving a page property value should succeed");
    Require(!cppwiki::storage::ExportWorkspaceToFile(source_repository, "engineering",
                                                     archive_path.string())
                 .error,
            "exporting the workspace should succeed");
  }

  {
    std::ostringstream contents;
    {
      std::ifstream in(archive_path, std::ios::binary);
      contents << in.rdbuf();
    }
    const auto raw = contents.str();
    const std::string broken = "\"property_definition_id\":\"property-missing\"";
    const std::string intact = "\"property_definition_id\":\"property-status\"";
    Require(raw.find(intact) != std::string::npos,
            "exported archive should reference the intact property definition id");
    {
      std::ofstream out(archive_path, std::ios::binary | std::ios::trunc);
      auto tampered = raw;
      const auto pos = tampered.find(intact);
      tampered.replace(pos, intact.size(), broken);
      out << tampered;
    }
  }

  cppwiki::storage::FileDocumentRepository repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = dest_dir});

  const auto result = cppwiki::storage::ImportWorkspaceFromFile(repository, archive_path.string());
  Require(result.error.has_value() &&
              result.error->code == cppwiki::storage::RepositoryErrorCode::kInvalidRecord,
          "importing an archive with a dangling knowledge reference should fail");
  // Rejecting the archive must not partially persist the orphaned value.
  Require(repository.ListPagePropertyValues("engineering", "page-1").values.empty(),
          "a dangling value must not be persisted");

  std::filesystem::remove_all(source_dir);
  std::filesystem::remove_all(dest_dir);
  std::filesystem::remove(archive_path);
}

auto TestImportRejectsInvalidArchiveFile() -> void {
  const auto dest_dir =
      std::filesystem::temp_directory_path() / "cppwiki-workspace-archive-test-invalid";
  const auto bogus_path =
      std::filesystem::temp_directory_path() / "cppwiki-workspace-archive-bogus.json";
  std::filesystem::remove_all(dest_dir);
  {
    std::ofstream bogus(bogus_path, std::ios::binary | std::ios::trunc);
    bogus << "not json at all";
  }

  cppwiki::storage::FileDocumentRepository repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = dest_dir});

  const auto result = cppwiki::storage::ImportWorkspaceFromFile(repository, bogus_path.string());
  Require(result.error.has_value(), "importing an invalid archive file should fail");

  std::filesystem::remove_all(dest_dir);
  std::filesystem::remove(bogus_path);
}

// Issue #185: the archive schema bumped to v2 when the knowledge arrays were added, but the
// reader must still tolerate an older v1 archive that predates them -- knowledge fields simply
// come back empty rather than failing the import. This writes a minimal v1 archive by hand
// (workspace + a document, no knowledge arrays at all) and asserts the import succeeds and
// restores the document, leaving knowledge empty -- exactly the "existing workspaces have no
// knowledge records and remain valid without a migration rewrite" rule from the contract.
auto TestImportAcceptsLegacyV1ArchiveWithoutKnowledgeFields() -> void {
  const auto dest_dir =
      std::filesystem::temp_directory_path() / "cppwiki-workspace-archive-test-legacy-v1";
  const auto archive_path =
      std::filesystem::temp_directory_path() / "cppwiki-workspace-archive-legacy-v1.json";
  std::filesystem::remove_all(dest_dir);
  std::filesystem::remove(archive_path);

  // Hand-written v1 shape: no knowledge arrays at all. Reflect-cpp must tolerate the absent
  // (default-valued / optional) knowledge fields and populate empty collections for them. The
  // document uses the exact serialized enum spelling reflect-cpp emits ("wikiPage", not the
  // snake_case source identifier) and carries the pre-existing required attachment array.
  const std::string legacy_v1 =
      R"({"archive_schema_version":1,)"
      R"("workspace":{"workspace_id":"engineering","title":"Engineering",)"
      R"("created_at":"2026-06-30T09:00:00.000Z","schema_version":1},)"
      R"("documents":[{"id":"page-1","schema_version":1,"kind":"wikiPage","title":"Legacy Page",)"
      R"("workspace_id":"engineering","parent_id":null,"sort_order":1,)"
      R"("created_at":"2026-06-30T10:00:00.000Z","updated_at":"2026-06-30T10:05:00.000Z",)"
      R"("created_by":"creator","updated_by":"editor","content_version":1,)"
      R"("raw_snapshot_json":"{\"id\":\"page-1\",\"schema_version\":1,\"title\":\"Legacy\",\"blocks\":[]}"}],)"
      R"("conflicts":[],"attachments":[]})";
  {
    std::ofstream out(archive_path, std::ios::binary | std::ios::trunc);
    out << legacy_v1;
  }

  cppwiki::storage::FileDocumentRepository repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = dest_dir});

  const auto result = cppwiki::storage::ImportWorkspaceFromFile(repository, archive_path.string());
  Require(!result.error, "a legacy v1 archive without knowledge fields should still import");
  Require(result.workspace_id.has_value() && *result.workspace_id == "engineering",
          "a legacy v1 archive should restore its workspace id");

  const auto restored = repository.LoadDocument("page-1");
  Require(restored.document.has_value(),
          "a legacy v1 archive should restore its document");
  Require(repository.ListPropertyDefinitions("engineering").definitions.empty(),
          "a legacy v1 archive should import with empty property definitions");
  Require(repository.ListRelationTypes("engineering").relation_types.empty(),
          "a legacy v1 archive should import with empty relation types");

  std::filesystem::remove_all(dest_dir);
  std::filesystem::remove(archive_path);
}

}  // namespace

auto main() -> int {
  TestExportImportRoundTripsDocumentsAndConflicts();
  TestExportImportRoundTripsKnowledgeRecords();
  TestImportRejectsDanglingKnowledgeReference();
  TestImportAcceptsLegacyV1ArchiveWithoutKnowledgeFields();
  TestImportRejectsInvalidArchiveFile();
  spdlog::info("cppwiki_workspace_archive_tests passed");
  return EXIT_SUCCESS;
}
