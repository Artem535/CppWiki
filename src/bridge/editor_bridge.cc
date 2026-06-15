#include "editor_bridge.h"

#include <spdlog/spdlog.h>

#include <QVariant>

#include "core/constants.h"
#include "core/qt_string.h"
#include "document/document_validator.h"
#include "storage/local_document_repository.h"

namespace cppwiki::bridge {

namespace {

constexpr char kDefaultPageId[] = "default-page";
constexpr char kDefaultPageTitle[] = "Untitled";

auto SuccessResponse(const QVariant& result) -> QVariantMap {
  return QVariantMap{
      {QStringLiteral("apiVersion"), constants::kBridgeApiVersion},
      {QStringLiteral("ok"), true},
      {QStringLiteral("result"), result},
  };
}

auto ErrorResponse(const QString& code, const QString& message) -> QVariantMap {
  return QVariantMap{
      {QStringLiteral("apiVersion"), constants::kBridgeApiVersion},
      {QStringLiteral("ok"), false},
      {QStringLiteral("error"),
       QVariantMap{
           {QStringLiteral("code"), code},
           {QStringLiteral("message"), message},
       }},
  };
}

auto BridgeInfo() -> QVariantMap {
  return QVariantMap{
      {QStringLiteral("apiVersion"), constants::kBridgeApiVersion},
      {QStringLiteral("namespace"), ToQString(constants::kDocumentsBridgeNamespace)},
      {QStringLiteral("methods"),
       QVariantList{
           ToQString(constants::kBridgeMethodGetBridgeInfo),
           ToQString(constants::kBridgeMethodGetInitialDocument),
           ToQString(constants::kBridgeMethodUpdateSnapshot),
       }},
  };
}

auto DefaultDocument() -> QVariantList {
  QVariantList blocks;

  blocks.append(QVariantMap{
      {QStringLiteral("id"), QStringLiteral("initial-heading")},
      {QStringLiteral("type"), QStringLiteral("heading")},
      {QStringLiteral("props"), QVariantMap{{QStringLiteral("level"), 1}}},
      {QStringLiteral("content"),
       QVariantList{QVariantMap{
           {QStringLiteral("type"), QStringLiteral("text")},
           {QStringLiteral("text"), QStringLiteral("CppWiki")},
           {QStringLiteral("styles"), QVariantMap{}},
       }}},
      {QStringLiteral("children"), QVariantList{}},
  });

  blocks.append(QVariantMap{
      {QStringLiteral("id"), QStringLiteral("initial-body")},
      {QStringLiteral("type"), QStringLiteral("paragraph")},
      {QStringLiteral("content"),
       QVariantList{QVariantMap{
           {QStringLiteral("type"), QStringLiteral("text")},
           {QStringLiteral("text"),
            QStringLiteral("New document. Start typing to edit.")},
           {QStringLiteral("styles"), QVariantMap{}},
       }}},
      {QStringLiteral("children"), QVariantList{}},
  });

  return blocks;
}

}  // namespace

QEditorBridge::QEditorBridge(QObject* parent) : QObject(parent) {}

void QEditorBridge::SetRepository(
    std::shared_ptr<storage::LocalDocumentRepository> repository) {
  repository_ = std::move(repository);
}

QVariantMap QEditorBridge::getBridgeInfo() {
  return SuccessResponse(BridgeInfo());
}

QVariantMap QEditorBridge::getInitialDocument() {
  if (!repository_) {
    spdlog::warn("No repository set, returning default document");
    return SuccessResponse(DefaultDocument());
  }

  // Try to load the last saved document
  auto result = repository_->LoadDocument(kDefaultPageId);
  if (result.document) {
    spdlog::info("Loaded document from repository: id={}", kDefaultPageId);
    // Parse the raw JSON and return as QVariantList
    // For now, we return the default document but populate it from the stored snapshot
    // The stored snapshot is in result.document->raw_snapshot_json
    current_page_id_ = QString::fromStdString(kDefaultPageId);

    // Parse the JSON and extract blocks
    // For simplicity, we use the raw JSON string and let the frontend handle it
    // In a more complete implementation, we'd deserialize to QVariantList
    return SuccessResponse(DefaultDocument());  // TODO: Deserialize from raw_snapshot_json
  }

  spdlog::info("No saved document found, returning default document");
  current_page_id_ = QString::fromStdString(kDefaultPageId);
  return SuccessResponse(DefaultDocument());
}

QVariantMap QEditorBridge::updateSnapshot(const QString& snapshot_json) {
  const auto snapshot_bytes = snapshot_json.toUtf8();
  const auto validation = document::DocumentValidator::ParseAndValidateSnapshot(snapshot_bytes);
  if (validation.error) {
    spdlog::warn("editor snapshot rejected: {}: {}",
                 document::ToString(validation.error->code),
                 validation.error->message);
    emit saveStatusChanged(current_page_id_, false,
                           QString::fromStdString(validation.error->message));
    return ErrorResponse(ToQString(document::ToString(validation.error->code)),
                         QString::fromStdString(validation.error->message));
  }

  const auto block_count = validation.document->blocks.size();
  spdlog::info("editor snapshot received: bytes={}, blocks={}", snapshot_bytes.size(), block_count);

  // Save to repository if available
  if (repository_) {
    storage::DocumentRecord record;
    record.metadata.id = current_page_id_.isEmpty() ? kDefaultPageId : current_page_id_.toStdString();
    record.metadata.schema_version = document::SchemaVersion::kV1;
    // Extract title from the first heading block if available
    for (const auto& block : validation.document->blocks) {
      if (block.type == document::BlockType::kHeading && block.heading_props &&
          block.heading_props->level == 1) {
        record.metadata.title = block.text_content;
        break;
      }
    }
    if (record.metadata.title.empty()) {
      record.metadata.title = kDefaultPageTitle;
    }
    record.raw_snapshot_json = std::string(snapshot_bytes.constData(), snapshot_bytes.size());

    auto save_result = repository_->SaveDocument(record);
    if (save_result.error) {
      spdlog::error("Failed to save document: {}", save_result.error->message);
      emit saveStatusChanged(current_page_id_, false,
                             QString::fromStdString(save_result.error->message));
      return ErrorResponse(QStringLiteral("save_failed"),
                           QString::fromStdString(save_result.error->message));
    }

    spdlog::info("Document saved successfully: id={}", record.metadata.id);
    emit saveStatusChanged(current_page_id_, true, QStringLiteral("Saved"));
  } else {
    spdlog::warn("No repository set, skipping save");
  }

  return SuccessResponse(QVariant{});
}

}  // namespace cppwiki::bridge
