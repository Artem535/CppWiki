#include "ai_chat/ai_chat_tool_registry.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <algorithm>
#include <cctype>
#include <utility>

#include "core/qt_string.h"
#include "knowledge/context_pack_composer.h"

namespace cppwiki::ai_chat {

namespace {

using agent::AgentToolInvocationResult;
using agent::AgentToolSchema;

auto ParseArguments(const std::string& arguments_json) -> QJsonObject {
  const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(arguments_json));
  return document.isObject() ? document.object() : QJsonObject{};
}

// Converts QEditorBridge's {"apiVersion", "ok", "result"|"error"} envelope into the Agent
// engine's plain success/error result shape.
auto ToToolResult(const QVariantMap& response) -> AgentToolInvocationResult {
  if (response.value(QStringLiteral("ok")).toBool()) {
    return AgentToolInvocationResult::Ok(
        QString::fromUtf8(QJsonDocument::fromVariant(response.value(QStringLiteral("result")))
                              .toJson(QJsonDocument::Compact))
            .toStdString());
  }
  const auto error = response.value(QStringLiteral("error")).toMap();
  return AgentToolInvocationResult::Error(
      error.value(QStringLiteral("message")).toString().toStdString());
}

auto ToLowerCopy(std::string text) -> std::string {
  std::ranges::transform(text, text.begin(),
                         [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return text;
}

auto Contains(const std::string& haystack, const std::string& needle) -> bool {
  return ToLowerCopy(haystack).find(ToLowerCopy(needle)) != std::string::npos;
}

}  // namespace

AiChatToolRegistry::AiChatToolRegistry(agent::AgentToolRegistry& registry,
                                       std::shared_ptr<storage::LocalDocumentRepository> repository,
                                       bridge::QEditorBridge& editor_bridge,
                                       document::DocumentEditabilityGate& gate,
                                       agent::AgentToolConfirmationProvider& confirmation_provider)
    : registry_(registry),
      repository_(std::move(repository)),
      editor_bridge_(editor_bridge),
      gate_(gate),
      confirmation_provider_(confirmation_provider) {}

void AiChatToolRegistry::RegisterTools() {
  RegisterListDocuments();
  RegisterReadDocument();
  RegisterSearchDocuments();
  RegisterUpdateDocumentSnapshot();
  RegisterRenameDocument();
  RegisterMoveDocument();
  RegisterDeleteDocument();
}

void AiChatToolRegistry::RegisterListDocuments() {
  registry_.RegisterTool(
      AgentToolSchema{.name = "list_documents",
                      .description = "List every wiki document's id and title.",
                      .parameters_json_schema = R"({"type":"object","properties":{}})"},
      [this](const std::string&) -> AgentToolInvocationResult {
        const auto listed = repository_->ListDocuments();
        if (listed.error) {
          return AgentToolInvocationResult::Error(listed.error->message);
        }
        QJsonArray array;
        for (const auto& summary : listed.documents) {
          QJsonObject entry;
          entry.insert(QStringLiteral("id"), ToQString(summary.id));
          entry.insert(QStringLiteral("title"), ToQString(summary.title));
          array.append(entry);
        }
        return AgentToolInvocationResult::Ok(
            QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)).toStdString());
      });
}

void AiChatToolRegistry::RegisterReadDocument() {
  registry_.RegisterTool(
      AgentToolSchema{
          .name = "read_document",
          .description = "Read one wiki document's title and plain-text content by id.",
          .parameters_json_schema =
              R"({"type":"object","properties":{"document_id":{"type":"string"}},"required":["document_id"]})"},
      [this](const std::string& arguments_json) -> AgentToolInvocationResult {
        const auto arguments = ParseArguments(arguments_json);
        const auto document_id = arguments.value(QStringLiteral("document_id")).toString();
        if (document_id.isEmpty()) {
          return AgentToolInvocationResult::Error("document_id is required.");
        }

        const auto loaded = repository_->LoadDocument(document_id.toStdString());
        if (!loaded.document) {
          return AgentToolInvocationResult::Error("Document was not found.");
        }

        QJsonObject result;
        result.insert(QStringLiteral("id"), ToQString(loaded.document->metadata.id));
        result.insert(QStringLiteral("title"), ToQString(loaded.document->metadata.title));
        result.insert(QStringLiteral("text"),
                      ToQString(knowledge::ExtractPlainText(loaded.document->snapshot)));
        return AgentToolInvocationResult::Ok(
            QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact)).toStdString());
      });
}

void AiChatToolRegistry::RegisterSearchDocuments() {
  registry_.RegisterTool(
      AgentToolSchema{
          .name = "search_documents",
          .description = "Find wiki documents whose title or content contains a substring. "
                         "A simple substring match, not a ranked search.",
          .parameters_json_schema =
              R"({"type":"object","properties":{"query":{"type":"string"}},"required":["query"]})"},
      [this](const std::string& arguments_json) -> AgentToolInvocationResult {
        const auto arguments = ParseArguments(arguments_json);
        const auto query = arguments.value(QStringLiteral("query")).toString().toStdString();
        if (query.empty()) {
          return AgentToolInvocationResult::Error("query is required.");
        }

        const auto listed = repository_->ListDocuments();
        if (listed.error) {
          return AgentToolInvocationResult::Error(listed.error->message);
        }

        QJsonArray matches;
        for (const auto& summary : listed.documents) {
          bool matched = Contains(summary.title, query);
          if (!matched) {
            const auto loaded = repository_->LoadDocument(summary.id);
            if (loaded.document) {
              matched = Contains(knowledge::ExtractPlainText(loaded.document->snapshot), query);
            }
          }
          if (matched) {
            QJsonObject entry;
            entry.insert(QStringLiteral("id"), ToQString(summary.id));
            entry.insert(QStringLiteral("title"), ToQString(summary.title));
            matches.append(entry);
          }
        }
        return AgentToolInvocationResult::Ok(
            QString::fromUtf8(QJsonDocument(matches).toJson(QJsonDocument::Compact)).toStdString());
      });
}

void AiChatToolRegistry::RunGatedMutation(const QString& document_id, std::string tool_name,
                                          std::string summary,
                                          std::function<AgentToolInvocationResult()> apply,
                                          std::function<void(AgentToolInvocationResult)> on_done) {
  gate_.Check(document_id, [this, tool_name = std::move(tool_name), summary = std::move(summary),
                            apply = std::move(apply), on_done = std::move(on_done)](
                               document::EditabilityCheckResult check) mutable {
    if (!check.editable) {
      on_done(AgentToolInvocationResult::Error(
          check.has_conflict
              ? "Document has an unresolved sync conflict. Resolve it before editing."
              : "Document is locked by another party."));
      return;
    }

    confirmation_provider_.RequestConfirmation(
        agent::ToolCallConfirmationRequest{.tool_name = tool_name, .summary = summary},
        [apply = std::move(apply), on_done = std::move(on_done)](bool approved) mutable {
          if (!approved) {
            on_done(AgentToolInvocationResult::Error("User declined this action."));
            return;
          }
          on_done(apply());
        });
  });
}

void AiChatToolRegistry::RegisterUpdateDocumentSnapshot() {
  registry_.RegisterAsyncTool(
      AgentToolSchema{
          .name = "update_document_snapshot",
          .description = "Replace a wiki document's content with new BlockNote block JSON.",
          .parameters_json_schema =
              R"({"type":"object","properties":{"document_id":{"type":"string"},)"
              R"("snapshot_json":{"type":"string"}},"required":["document_id","snapshot_json"]})"},
      [this](const std::string& arguments_json,
             std::function<void(AgentToolInvocationResult)> on_done) {
        const auto arguments = ParseArguments(arguments_json);
        const auto document_id = arguments.value(QStringLiteral("document_id")).toString();
        const auto snapshot_json = arguments.value(QStringLiteral("snapshot_json")).toString();
        if (document_id.isEmpty() || snapshot_json.isEmpty()) {
          on_done(AgentToolInvocationResult::Error("document_id and snapshot_json are required."));
          return;
        }

        RunGatedMutation(
            document_id, "update_document_snapshot",
            "Update the content of document " + document_id.toStdString(),
            [this, document_id, snapshot_json]() {
              return ToToolResult(
                  editor_bridge_.SeedNewDocumentRawContent(document_id, snapshot_json));
            },
            std::move(on_done));
      });
}

void AiChatToolRegistry::RegisterRenameDocument() {
  registry_.RegisterAsyncTool(
      AgentToolSchema{.name = "rename_document",
                      .description = "Rename a wiki document.",
                      .parameters_json_schema =
                          R"({"type":"object","properties":{"document_id":{"type":"string"},)"
                          R"("title":{"type":"string"}},"required":["document_id","title"]})"},
      [this](const std::string& arguments_json,
             std::function<void(AgentToolInvocationResult)> on_done) {
        const auto arguments = ParseArguments(arguments_json);
        const auto document_id = arguments.value(QStringLiteral("document_id")).toString();
        const auto title = arguments.value(QStringLiteral("title")).toString();
        if (document_id.isEmpty() || title.isEmpty()) {
          on_done(AgentToolInvocationResult::Error("document_id and title are required."));
          return;
        }

        RunGatedMutation(
            document_id, "rename_document",
            "Rename document " + document_id.toStdString() + " to \"" + title.toStdString() + "\"",
            [this, document_id, title]() {
              return ToToolResult(editor_bridge_.renameDocument(document_id, title));
            },
            std::move(on_done));
      });
}

void AiChatToolRegistry::RegisterMoveDocument() {
  registry_.RegisterAsyncTool(
      AgentToolSchema{
          .name = "move_document",
          .description = "Move a wiki document to a new parent (or to the workspace root) and "
                         "sort position.",
          .parameters_json_schema =
              R"({"type":"object","properties":{"document_id":{"type":"string"},)"
              R"("parent_id":{"type":["string","null"]},"sort_order":{"type":"integer"}},)"
              R"("required":["document_id","sort_order"]})"},
      [this](const std::string& arguments_json,
             std::function<void(AgentToolInvocationResult)> on_done) {
        const auto arguments = ParseArguments(arguments_json);
        const auto document_id = arguments.value(QStringLiteral("document_id")).toString();
        if (document_id.isEmpty()) {
          on_done(AgentToolInvocationResult::Error("document_id is required."));
          return;
        }
        const auto parent_id_value = arguments.value(QStringLiteral("parent_id"));
        const bool has_parent_id =
            parent_id_value.isString() && !parent_id_value.toString().isEmpty();
        const auto parent_id = has_parent_id ? parent_id_value.toString() : QString();
        const auto sort_order = arguments.value(QStringLiteral("sort_order")).toInt();

        RunGatedMutation(
            document_id, "move_document", "Move document " + document_id.toStdString(),
            [this, document_id, parent_id, has_parent_id, sort_order]() {
              return ToToolResult(editor_bridge_.updateDocumentPlacement(
                  document_id, parent_id, has_parent_id, sort_order));
            },
            std::move(on_done));
      });
}

void AiChatToolRegistry::RegisterDeleteDocument() {
  registry_.RegisterAsyncTool(
      AgentToolSchema{.name = "delete_document",
                      .description = "Move a wiki document (and its descendants) to the trash.",
                      .parameters_json_schema =
                          R"({"type":"object","properties":{"document_id":{"type":"string"}},)"
                          R"("required":["document_id"]})"},
      [this](const std::string& arguments_json,
             std::function<void(AgentToolInvocationResult)> on_done) {
        const auto arguments = ParseArguments(arguments_json);
        const auto document_id = arguments.value(QStringLiteral("document_id")).toString();
        if (document_id.isEmpty()) {
          on_done(AgentToolInvocationResult::Error("document_id is required."));
          return;
        }

        RunGatedMutation(
            document_id, "delete_document", "Delete document " + document_id.toStdString(),
            [this, document_id]() {
              return ToToolResult(editor_bridge_.deleteDocument(document_id));
            },
            std::move(on_done));
      });
}

}  // namespace cppwiki::ai_chat
