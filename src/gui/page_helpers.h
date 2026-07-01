#ifndef CPPWIKI_SRC_GUI_PAGE_HELPERS_H_
#define CPPWIKI_SRC_GUI_PAGE_HELPERS_H_

#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include "app/app_context.h"
#include "storage/local_document_repository.h"
#include "sync/sync_bootstrap.h"

namespace cppwiki::gui::page_helpers {

// Returns the string value of the first present key in `map`, or std::nullopt
// if the value is missing/invalid/empty after trimming.
auto OptionalString(const QVariant& value) -> std::optional<std::string>;

// Looks up the first key from `keys` that exists in `map` and returns its value.
auto ValueFromFirstExistingKey(const QVariantMap& map, std::initializer_list<QString> keys)
    -> QVariant;

// Extracts the "parent_id"/"parentId" (snake_case or camelCase) value from a document map.
auto OptionalParentId(const QVariantMap& document) -> std::optional<std::string>;

auto StringFromFirstExistingKey(const QVariantMap& map, std::initializer_list<QString> keys)
    -> QString;

auto IntFromFirstExistingKey(const QVariantMap& map, std::initializer_list<QString> keys,
                             int default_value = 0) -> int;

// Extracts the workspace id encoded in a "workspace:<id>" sync channel, defaulting to "default".
auto WorkspaceIdFromBootstrap(const sync::SyncBootstrap& bootstrap) -> QString;

// Extracts all workspace ids encoded as "workspace:<id>" sync channels.
auto WorkspaceIdsFromBootstrap(const sync::SyncBootstrap& bootstrap) -> QStringList;

// Extracts author id (principal subject/username/email, in that priority) from bootstrap.
auto AuthorIdFromBootstrap(const sync::SyncBootstrap& bootstrap) -> QString;

// Computes the effective list of workspace ids visible to the current session,
// preferring the document sync service snapshot, falling back to backend bootstrap.
auto EffectiveWorkspaceIds(const AppContext& context) -> QStringList;

// Computes the effective author id for document authoring attribution.
auto EffectiveAuthorId(const AppContext& context) -> QString;

// Picks the preferred workspace id (bootstrap hint if available among the given list).
auto PreferredWorkspaceId(const AppContext& context, const QStringList& available_workspace_ids)
    -> QString;

// Builds a workspace-scoped key used to track expanded tree state across rebuilds.
auto MakeWorkspaceScopedDocumentId(const QString& workspace_id, std::string_view document_id)
    -> std::string;

// Recursively visits all model indexes rooted at `parent`, calling `visitor` for each.
void VisitIndexes(const QAbstractItemModel* model, const QModelIndex& parent,
                  const std::function<void(const QModelIndex&)>& visitor);

// Compares two document summary lists field-by-field, ignoring vector identity.
auto AreDocumentSummariesEqual(const std::vector<storage::DocumentSummary>& lhs,
                               const std::vector<storage::DocumentSummary>& rhs) -> bool;

// Returns true if the current session/backend allows creating new workspaces.
auto CanCreateWorkspace(const AppContext& context) -> bool;

// Converts a QVariantMap document representation (from the JS bridge) into a DocumentSummary.
auto SummaryFromVariantMap(const QVariantMap& document) -> storage::DocumentSummary;

}  // namespace cppwiki::gui::page_helpers

#endif  // CPPWIKI_SRC_GUI_PAGE_HELPERS_H_
