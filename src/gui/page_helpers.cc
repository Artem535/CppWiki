#include "gui/page_helpers.h"

#include <QJsonDocument>
#include <QJsonObject>

#include "backend/backend_client.h"
#include "sync/sync_service.h"

namespace cppwiki::gui::page_helpers {

auto OptionalString(const QVariant& value) -> std::optional<std::string> {
  if (!value.isValid() || value.isNull()) {
    return std::nullopt;
  }

  const auto str = value.toString().trimmed();
  if (str.isEmpty()) {
    return std::nullopt;
  }

  return str.toStdString();
}

auto ValueFromFirstExistingKey(const QVariantMap& map, std::initializer_list<QString> keys)
    -> QVariant {
  for (const auto& key : keys) {
    if (map.contains(key)) {
      return map.value(key);
    }
  }

  return {};
}

auto OptionalParentId(const QVariantMap& document) -> std::optional<std::string> {
  // UI / bridge usually uses camelCase, while storage / JSON may use snake_case.
  return OptionalString(ValueFromFirstExistingKey(
      document, {QStringLiteral("parentId"), QStringLiteral("parent_id"),
                 QStringLiteral("parentDocumentId"), QStringLiteral("parent_document_id")}));
}

auto StringFromFirstExistingKey(const QVariantMap& map, std::initializer_list<QString> keys)
    -> QString {
  return ValueFromFirstExistingKey(map, keys).toString();
}

auto IntFromFirstExistingKey(const QVariantMap& map, std::initializer_list<QString> keys,
                             int default_value) -> int {
  const auto value = ValueFromFirstExistingKey(map, keys);
  if (!value.isValid() || value.isNull()) {
    return default_value;
  }

  return value.toInt();
}

auto WorkspaceIdFromBootstrap(const sync::SyncBootstrap& bootstrap) -> QString {
  for (const auto& channel : bootstrap.channels) {
    const auto trimmed = channel.trimmed();
    if (trimmed.startsWith(QStringLiteral("workspace:"))) {
      const auto workspace_id = trimmed.sliced(QStringLiteral("workspace:").size()).trimmed();
      if (!workspace_id.isEmpty()) {
        return workspace_id;
      }
    }
  }
  return QStringLiteral("default");
}

auto WorkspaceIdsFromBootstrap(const sync::SyncBootstrap& bootstrap) -> QStringList {
  QStringList workspace_ids;
  for (const auto& channel : bootstrap.channels) {
    const auto trimmed = channel.trimmed();
    if (!trimmed.startsWith(QStringLiteral("workspace:"))) {
      continue;
    }

    const auto workspace_id = trimmed.sliced(QStringLiteral("workspace:").size()).trimmed();
    if (!workspace_id.isEmpty() && !workspace_ids.contains(workspace_id)) {
      workspace_ids.push_back(workspace_id);
    }
  }

  if (workspace_ids.isEmpty()) {
    workspace_ids.push_back(QStringLiteral("default"));
  }
  return workspace_ids;
}

auto AuthorIdFromBootstrap(const sync::SyncBootstrap& bootstrap) -> QString {
  if (!bootstrap.principal_subject.trimmed().isEmpty()) {
    return bootstrap.principal_subject.trimmed();
  }
  if (!bootstrap.principal_username.trimmed().isEmpty()) {
    return bootstrap.principal_username.trimmed();
  }
  if (!bootstrap.principal_email.trimmed().isEmpty()) {
    return bootstrap.principal_email.trimmed();
  }
  return {};
}

auto EffectiveWorkspaceIds(const AppContext& context) -> QStringList {
  if (context.document_sync_service != nullptr) {
    const auto& snapshot = context.document_sync_service->Snapshot();
    QStringList workspace_ids = snapshot.workspace_ids;
    for (const auto& workspace_id : snapshot.hydrated_workspace_ids) {
      if (!workspace_ids.contains(workspace_id)) {
        workspace_ids.push_back(workspace_id);
      }
    }
    if (!workspace_ids.isEmpty()) {
      return workspace_ids;
    }
  }

  if (context.backend_client != nullptr) {
    return WorkspaceIdsFromBootstrap(context.backend_client->CurrentSyncBootstrap());
  }

  return QStringList{QStringLiteral("default")};
}

auto EffectiveAuthorId(const AppContext& context) -> QString {
  if (context.document_sync_service != nullptr) {
    const auto author_id = AuthorIdFromBootstrap(context.document_sync_service->Snapshot().bootstrap);
    if (!author_id.trimmed().isEmpty()) {
      return author_id.trimmed();
    }
  }

  if (context.backend_client != nullptr) {
    return AuthorIdFromBootstrap(context.backend_client->CurrentSyncBootstrap()).trimmed();
  }

  return {};
}

auto PreferredWorkspaceId(const AppContext& context, const QStringList& available_workspace_ids)
    -> QString {
  if (context.document_sync_service != nullptr) {
    const auto preferred = WorkspaceIdFromBootstrap(context.document_sync_service->Snapshot().bootstrap);
    if (available_workspace_ids.contains(preferred)) {
      return preferred;
    }
  }

  if (context.backend_client != nullptr) {
    const auto preferred = WorkspaceIdFromBootstrap(context.backend_client->CurrentSyncBootstrap());
    if (available_workspace_ids.contains(preferred)) {
      return preferred;
    }
  }

  return available_workspace_ids.isEmpty() ? QStringLiteral("default")
                                           : available_workspace_ids.front();
}

auto MakeWorkspaceScopedDocumentId(const QString& workspace_id, std::string_view document_id)
    -> std::string {
  return QStringLiteral("%1::%2")
      .arg(workspace_id, QString::fromStdString(std::string(document_id)))
      .toStdString();
}

void VisitIndexes(const QAbstractItemModel* model, const QModelIndex& parent,
                  const std::function<void(const QModelIndex&)>& visitor) {
  if (model == nullptr) {
    return;
  }

  const int rows = model->rowCount(parent);
  for (int row = 0; row < rows; ++row) {
    const auto index = model->index(row, 0, parent);
    if (!index.isValid()) {
      continue;
    }
    visitor(index);
    VisitIndexes(model, index, visitor);
  }
}

auto AreDocumentSummariesEqual(const std::vector<storage::DocumentSummary>& lhs,
                               const std::vector<storage::DocumentSummary>& rhs) -> bool {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (std::size_t index = 0; index < lhs.size(); ++index) {
    const auto& left = lhs[index];
    const auto& right = rhs[index];
    if (left.id != right.id || left.kind != right.kind || left.title != right.title ||
        left.workspace_id != right.workspace_id || left.parent_id != right.parent_id ||
        left.sort_order != right.sort_order || left.created_at != right.created_at ||
        left.updated_at != right.updated_at || left.created_by != right.created_by ||
        left.updated_by != right.updated_by || left.content_version != right.content_version) {
      return false;
    }
  }

  return true;
}

auto SummaryFromVariantMap(const QVariantMap& document) -> storage::DocumentSummary {
  storage::DocumentSummary summary;
  summary.id = StringFromFirstExistingKey(document, {QStringLiteral("id")}).toStdString();
  summary.kind = document::DocumentKindFromKey(
      StringFromFirstExistingKey(document, {QStringLiteral("kind")}).toStdString());
  summary.title = StringFromFirstExistingKey(document, {QStringLiteral("title")}).toStdString();
  summary.workspace_id = StringFromFirstExistingKey(
      document, {QStringLiteral("workspaceId"), QStringLiteral("workspace_id")}).toStdString();
  summary.parent_id = OptionalParentId(document);
  summary.sort_order = IntFromFirstExistingKey(
      document, {QStringLiteral("sortOrder"), QStringLiteral("sort_order")});
  summary.created_at = StringFromFirstExistingKey(
      document, {QStringLiteral("createdAt"), QStringLiteral("created_at")}).toStdString();
  summary.updated_at = StringFromFirstExistingKey(
      document, {QStringLiteral("updatedAt"), QStringLiteral("updated_at")}).toStdString();
  summary.created_by = StringFromFirstExistingKey(
      document, {QStringLiteral("createdBy"), QStringLiteral("created_by")}).toStdString();
  summary.updated_by = StringFromFirstExistingKey(
      document, {QStringLiteral("updatedBy"), QStringLiteral("updated_by")}).toStdString();
  summary.content_version = ValueFromFirstExistingKey(
      document, {QStringLiteral("contentVersion"), QStringLiteral("content_version")}).toLongLong();
  if (summary.content_version < 1) {
    summary.content_version = 1;
  }
  return summary;
}

auto FileDialogNameFilterForKind(document::DocumentKind kind) -> QString {
  switch (kind) {
    case document::DocumentKind::kJupyterNotebook:
      return QStringLiteral("Jupyter Notebook (*.ipynb)");
    case document::DocumentKind::kExcalidrawCanvas:
      return QStringLiteral("Excalidraw scene (*.excalidraw)");
    case document::DocumentKind::kWikiPage:
      return QString();
  }
  return QString();
}

auto FileExtensionForKind(document::DocumentKind kind) -> QString {
  switch (kind) {
    case document::DocumentKind::kJupyterNotebook:
      return QStringLiteral("ipynb");
    case document::DocumentKind::kExcalidrawCanvas:
      return QStringLiteral("excalidraw");
    case document::DocumentKind::kWikiPage:
      return QString();
  }
  return QString();
}

auto ImportAnyKindNameFilter() -> QString {
  return QStringLiteral(
      "All supported files (*.ipynb *.excalidraw *.md *.markdown);;"
      "Jupyter Notebook (*.ipynb);;"
      "Excalidraw scene (*.excalidraw);;"
      "Markdown (*.md *.markdown)");
}

auto DetectImportableDocumentKind(const QString& file_name, const QString& content)
    -> std::optional<document::DocumentKind> {
  const auto parsed = QJsonDocument::fromJson(content.toUtf8());
  if (parsed.isObject()) {
    const auto object = parsed.object();
    const bool looks_like_notebook =
        object.value(QStringLiteral("cells")).isArray() &&
        object.value(QStringLiteral("nbformat")).isDouble();
    if (looks_like_notebook) {
      return document::DocumentKind::kJupyterNotebook;
    }

    const bool looks_like_canvas = object.value(QStringLiteral("elements")).isArray() &&
        object.value(QStringLiteral("appState")).isObject() &&
        object.value(QStringLiteral("files")).isObject();
    if (looks_like_canvas) {
      return document::DocumentKind::kExcalidrawCanvas;
    }
  }

  const auto lower_name = file_name.toLower();
  if (lower_name.endsWith(QStringLiteral(".md")) || lower_name.endsWith(QStringLiteral(".markdown"))) {
    return document::DocumentKind::kWikiPage;
  }

  return std::nullopt;
}

}  // namespace cppwiki::gui::page_helpers
