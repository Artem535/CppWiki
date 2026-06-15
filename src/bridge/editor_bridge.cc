#include "editor_bridge.h"

#include <spdlog/spdlog.h>

#include <QVariant>

#include "core/constants.h"
#include "core/qt_string.h"
#include "document/document_validator.h"

namespace cppwiki::bridge {

namespace {

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

auto InitialDocument() -> QVariantList {
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
            QStringLiteral("Initial document loaded from C++ through QWebChannel.")},
           {QStringLiteral("styles"), QVariantMap{}},
       }}},
      {QStringLiteral("children"), QVariantList{}},
  });

  return blocks;
}

}  // namespace

QEditorBridge::QEditorBridge(QObject* parent) : QObject(parent) {}

QVariantMap QEditorBridge::getBridgeInfo() {
  return SuccessResponse(BridgeInfo());
}

QVariantMap QEditorBridge::getInitialDocument() {
  return SuccessResponse(InitialDocument());
}

QVariantMap QEditorBridge::updateSnapshot(const QString& snapshot_json) {
  const auto snapshot_bytes = snapshot_json.toUtf8();
  const auto validation = document::DocumentValidator::ParseAndValidateSnapshot(snapshot_bytes);
  if (validation.error) {
    spdlog::warn("editor snapshot rejected: {}: {}",
                 ToString(validation.error->code),
                 validation.error->message);
    return ErrorResponse(ToQString(ToString(validation.error->code)),
                         QString::fromStdString(validation.error->message));
  }

  const auto block_count = validation.document->blocks.size();

  spdlog::info("editor snapshot received: bytes={}, blocks={}", snapshot_bytes.size(), block_count);

  return SuccessResponse(QVariant{});
}

}  // namespace cppwiki::bridge
