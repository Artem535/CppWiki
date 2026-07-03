#include "gui/merge/conflict_merge_model.h"

#include <spdlog/spdlog.h>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariant>

#include <cstdlib>
#include <optional>
#include <string_view>

namespace {

using cppwiki::gui::merge::ConflictMergeModel;

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto MakeConflict(std::string local_snapshot, std::string remote_snapshot)
    -> cppwiki::storage::DocumentConflictRecord {
  return cppwiki::storage::DocumentConflictRecord{
      .id = "conflict-1",
      .document_id = "page-1",
      .workspace_id = "default",
      .base_version = 3,
      .local_snapshot = std::move(local_snapshot),
      .remote_snapshot = std::move(remote_snapshot),
      .local_updated_by = "alice",
      .remote_updated_by = "bob",
      .detected_at = "2026-07-02T10:00:00.000Z",
      .resolution_state = "pending",
  };
}

auto TestLoadConflictMatchesBlocksByStableId() -> void {
  ConflictMergeModel model;
  const auto loaded = model.LoadConflict(
      MakeConflict(
          R"({"id":"page-1","schema_version":1,"title":"Local","blocks":[{"id":"a","type":"paragraph","content":[{"type":"text","text":"Local A","styles":{}}],"children":[]},{"id":"b","type":"paragraph","content":[{"type":"text","text":"Local B","styles":{}}],"children":[]}]})",
          R"({"id":"page-1","schema_version":1,"title":"Remote","blocks":[{"id":"b","type":"paragraph","content":[{"type":"text","text":"Remote B","styles":{}}],"children":[]},{"id":"a","type":"paragraph","content":[{"type":"text","text":"Remote A","styles":{}}],"children":[]}]})"),
      std::nullopt);

  Require(loaded, "conflict model should load reordered conflict");
  Require(model.rowCount() == 2, "reordered blocks should still produce two rows");
  Require(model.data(model.index(0, 0), ConflictMergeModel::BlockIdRole).toString() ==
              QStringLiteral("a"),
          "first row should stay aligned to local block a");
  Require(model.data(model.index(0, 0), ConflictMergeModel::RemotePreviewRole).toString() ==
              QStringLiteral("paragraph: Remote A"),
          "remote preview for block a should come from matching id, not remote index");
  Require(model.data(model.index(1, 0), ConflictMergeModel::BlockIdRole).toString() ==
              QStringLiteral("b"),
          "second row should stay aligned to local block b");
  Require(model.data(model.index(1, 0), ConflictMergeModel::RemotePreviewRole).toString() ==
              QStringLiteral("paragraph: Remote B"),
          "remote preview for block b should come from matching id, not remote index");
}

auto TestLoadConflictAppendsRemoteInsertions() -> void {
  ConflictMergeModel model;
  const auto loaded = model.LoadConflict(
      MakeConflict(
          R"({"id":"page-1","schema_version":1,"title":"Local","blocks":[{"id":"a","type":"paragraph","content":[{"type":"text","text":"Local A","styles":{}}],"children":[]},{"id":"b","type":"paragraph","content":[{"type":"text","text":"Local B","styles":{}}],"children":[]}]})",
          R"({"id":"page-1","schema_version":1,"title":"Remote","blocks":[{"id":"x","type":"paragraph","content":[{"type":"text","text":"Inserted X","styles":{}}],"children":[]},{"id":"a","type":"paragraph","content":[{"type":"text","text":"Remote A","styles":{}}],"children":[]},{"id":"b","type":"paragraph","content":[{"type":"text","text":"Remote B","styles":{}}],"children":[]}]})"),
      std::nullopt);

  Require(loaded, "conflict model should load remote insertion conflict");
  Require(model.rowCount() == 3, "unmatched remote insert should add a third row");
  Require(model.data(model.index(2, 0), ConflictMergeModel::BlockIdRole).toString() ==
              QStringLiteral("x"),
          "unmatched remote block should be appended as its own row");
  Require(!model.data(model.index(2, 0), ConflictMergeModel::HasLocalRole).toBool(),
          "appended remote insertion should not pretend to have a local side");
  Require(model.data(model.index(2, 0), ConflictMergeModel::HasRemoteRole).toBool(),
          "appended remote insertion should preserve the remote side");
  Require(model.data(model.index(2, 0), ConflictMergeModel::ResolutionRole).toString() ==
              QStringLiteral("remote"),
          "remote-only insertion should default to remote resolution");
}

auto TestBuildMergedDocumentKeepBothClonesDuplicateId() -> void {
  ConflictMergeModel model;
  const auto loaded = model.LoadConflict(
      MakeConflict(
          R"({"id":"page-1","schema_version":1,"title":"Local","blocks":[{"id":"a","type":"paragraph","content":[{"type":"text","text":"Local A","styles":{}}],"children":[]}]})",
          R"({"id":"page-1","schema_version":1,"title":"Remote","blocks":[{"id":"a","type":"paragraph","content":[{"type":"text","text":"Remote A","styles":{}}],"children":[]}]})"),
      std::nullopt);

  Require(loaded, "conflict model should load keep-both conflict");
  model.SetResolution(0, QStringLiteral("both"));
  const auto merged = model.BuildMergedDocument();
  Require(merged.has_value(), "merged document should be created");

  const auto blocks = merged->value(QStringLiteral("blocks")).toArray();
  Require(blocks.size() == 2, "keep both should preserve both blocks");
  Require(blocks.at(0).toObject().value(QStringLiteral("id")).toString() ==
              QStringLiteral("a"),
          "local block id should remain unchanged");
  Require(blocks.at(1).toObject().value(QStringLiteral("id")).toString() !=
              QStringLiteral("a"),
          "duplicated remote block should receive a fresh id");
}

auto TestBuildMergedDocumentUsesRemoteOrderWhenAllRowsResolveRemote() -> void {
  ConflictMergeModel model;
  const auto loaded = model.LoadConflict(
      MakeConflict(
          R"({"id":"page-1","schema_version":1,"title":"Local","blocks":[{"id":"a","type":"paragraph","content":[{"type":"text","text":"Local A","styles":{}}],"children":[]},{"id":"b","type":"paragraph","content":[{"type":"text","text":"Local B","styles":{}}],"children":[]}]})",
          R"({"id":"page-1","schema_version":1,"title":"Remote","blocks":[{"id":"b","type":"paragraph","content":[{"type":"text","text":"Remote B","styles":{}}],"children":[]},{"id":"a","type":"paragraph","content":[{"type":"text","text":"Remote A","styles":{}}],"children":[]}]})"),
      std::nullopt);

  Require(loaded, "conflict model should load remote order conflict");
  model.SetResolutionForAll(QStringLiteral("remote"));
  const auto merged = model.BuildMergedDocument();
  Require(merged.has_value(), "merged remote document should be created");

  const auto blocks = merged->value(QStringLiteral("blocks")).toArray();
  Require(blocks.size() == 2, "remote-only merge should keep two blocks");
  Require(blocks.at(0).toObject().value(QStringLiteral("id")).toString() ==
              QStringLiteral("b"),
          "remote-only merge should preserve remote block order");
  Require(blocks.at(1).toObject().value(QStringLiteral("id")).toString() ==
              QStringLiteral("a"),
          "remote-only merge should preserve remote block order for trailing blocks");
}

auto TestBuildMergedDocumentUsesRemoteTitleWhenAllRowsResolveRemote() -> void {
  ConflictMergeModel model;
  const auto loaded = model.LoadConflict(
      MakeConflict(
          R"({"id":"page-1","schema_version":1,"title":"Local Title","blocks":[{"id":"a","type":"paragraph","content":[{"type":"text","text":"Same","styles":{}}],"children":[]}]})",
          R"({"id":"page-1","schema_version":1,"title":"Remote Title","blocks":[{"id":"a","type":"paragraph","content":[{"type":"text","text":"Same","styles":{}}],"children":[]}]})"),
      std::nullopt);

  Require(loaded, "conflict model should load remote title conflict");
  model.SetResolutionForAll(QStringLiteral("remote"));
  const auto merged = model.BuildMergedDocument();
  Require(merged.has_value(), "merged remote title document should be created");
  Require(merged->value(QStringLiteral("title")).toString() == QStringLiteral("Remote Title"),
          "remote-only merge should preserve remote title");
}

auto TestBuildMergedDocumentUsesChosenSideOrderForMixedResolution() -> void {
  ConflictMergeModel model;
  const auto loaded = model.LoadConflict(
      MakeConflict(
          R"({"id":"page-1","schema_version":1,"title":"Local","blocks":[{"id":"a","type":"paragraph","content":[{"type":"text","text":"Local A","styles":{}}],"children":[]},{"id":"b","type":"paragraph","content":[{"type":"text","text":"Local B","styles":{}}],"children":[]}]})",
          R"({"id":"page-1","schema_version":1,"title":"Remote","blocks":[{"id":"b","type":"paragraph","content":[{"type":"text","text":"Remote B","styles":{}}],"children":[]},{"id":"a","type":"paragraph","content":[{"type":"text","text":"Remote A","styles":{}}],"children":[]}]})"),
      std::nullopt);

  Require(loaded, "conflict model should load mixed-order conflict");
  model.SetResolution(0, QStringLiteral("local"));
  model.SetResolution(1, QStringLiteral("remote"));
  const auto merged = model.BuildMergedDocument();
  Require(merged.has_value(), "mixed-order merged document should be created");

  const auto blocks = merged->value(QStringLiteral("blocks")).toArray();
  Require(blocks.size() == 2, "mixed-order merge should keep two blocks");
  Require(blocks.at(0).toObject().value(QStringLiteral("id")).toString() ==
              QStringLiteral("b"),
          "remote-selected moved block should be ordered using remote position");
  Require(blocks.at(1).toObject().value(QStringLiteral("id")).toString() ==
              QStringLiteral("a"),
          "local-selected block should remain after the moved remote-selected block");
}

}  // namespace

auto main(int argc, char** argv) -> int {
  QCoreApplication app(argc, argv);
  TestLoadConflictMatchesBlocksByStableId();
  TestLoadConflictAppendsRemoteInsertions();
  TestBuildMergedDocumentKeepBothClonesDuplicateId();
  TestBuildMergedDocumentUsesRemoteOrderWhenAllRowsResolveRemote();
  TestBuildMergedDocumentUsesRemoteTitleWhenAllRowsResolveRemote();
  TestBuildMergedDocumentUsesChosenSideOrderForMixedResolution();
  return EXIT_SUCCESS;
}
