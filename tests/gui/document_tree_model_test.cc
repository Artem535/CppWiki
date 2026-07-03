#include "gui/document_tree_model.h"

#include <QCoreApplication>
#include <QStringList>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

void TestEmptyAssignedWorkspacesAreVisible() {
  cppwiki::gui::DocumentTreeModel model;
  model.setWorkspaces({QStringLiteral("default"), QStringLiteral("engineering")});
  model.setDocuments({});

  Require(model.rowCount() == 2, "model should expose empty assigned workspaces");

  const auto default_index = model.indexForWorkspaceId("default");
  const auto engineering_index = model.indexForWorkspaceId("engineering");
  Require(default_index.isValid(), "default workspace should be visible without documents");
  Require(engineering_index.isValid(), "engineering workspace should be visible without documents");
  Require(model.isWorkspace(default_index), "default index should be a workspace");
  Require(model.isWorkspace(engineering_index), "engineering index should be a workspace");
}

void TestDocumentsAddMissingWorkspace() {
  cppwiki::gui::DocumentTreeModel model;
  model.setWorkspaces({QStringLiteral("default")});

  std::vector<cppwiki::storage::DocumentSummary> documents;
  documents.push_back(cppwiki::storage::DocumentSummary{
      .id = "page-1",
      .title = "Page",
      .workspace_id = "engineering",
      .parent_id = std::nullopt,
      .sort_order = 0,
      .created_at = "2026-07-03T00:00:00Z",
      .updated_at = "2026-07-03T00:00:00Z",
      .created_by = "tester",
      .updated_by = "tester",
      .content_version = 1,
  });
  model.setDocuments(documents);

  Require(model.indexForWorkspaceId("default").isValid(),
          "assigned empty default workspace should remain visible");
  Require(model.indexForWorkspaceId("engineering").isValid(),
          "workspace present only in documents should be visible");
}

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication application(argc, argv);

  TestEmptyAssignedWorkspacesAreVisible();
  TestDocumentsAddMissingWorkspace();

  return EXIT_SUCCESS;
}
