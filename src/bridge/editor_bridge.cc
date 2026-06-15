#include "editor_bridge.h"

#include <spdlog/spdlog.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QVariant>

#include "core/constants.h"
#include "core/qt_string.h"

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
      {QStringLiteral("type"), QStringLiteral("heading")},
      {QStringLiteral("props"), QVariantMap{{QStringLiteral("level"), 1}}},
      {QStringLiteral("content"), QStringLiteral("CppWiki")},
  });

  blocks.append(QVariantMap{
      {QStringLiteral("type"), QStringLiteral("paragraph")},
      {QStringLiteral("content"),
       QStringLiteral("Initial document loaded from C++ through QWebChannel.")},
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
  QJsonParseError parse_error;
  const auto document = QJsonDocument::fromJson(snapshot_bytes, &parse_error);
  if (parse_error.error != QJsonParseError::NoError) {
    spdlog::warn("editor snapshot rejected: invalid json: {}",
                 parse_error.errorString().toStdString());
    return ErrorResponse(QStringLiteral("invalid_json"),
                         QStringLiteral("Snapshot payload is not valid JSON."));
  }

  if (!document.isArray()) {
    spdlog::warn("editor snapshot rejected: root payload is not an array");
    return ErrorResponse(QStringLiteral("invalid_snapshot"),
                         QStringLiteral("Snapshot payload must be a BlockNote block array."));
  }

  const auto block_count = document.isArray() ? document.array().size() : 0;

  spdlog::info("editor snapshot received: bytes={}, blocks={}", snapshot_bytes.size(), block_count);

  return SuccessResponse(QVariant{});
}

}  // namespace cppwiki::bridge
