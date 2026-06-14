#include "editor_bridge.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QVariant>
#include <spdlog/spdlog.h>

namespace cppwiki::bridge {

namespace {

auto SuccessResponse(const QVariant& result) -> QVariantMap {
  return QVariantMap{
      {QStringLiteral("ok"), true},
      {QStringLiteral("result"), result},
  };
}

auto InitialDocument() -> QVariantList {
  QVariantList blocks;

  blocks.append(QVariantMap{
      {QStringLiteral("type"), QStringLiteral("heading")},
      {QStringLiteral("props"),
       QVariantMap{{QStringLiteral("level"), 1}}},
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

QVariantMap QEditorBridge::getInitialDocument() {
  return SuccessResponse(InitialDocument());
}

QVariantMap QEditorBridge::updateSnapshot(const QString& snapshot_json) {
  const auto snapshot_bytes = snapshot_json.toUtf8();
  const auto document = QJsonDocument::fromJson(snapshot_bytes);
  const auto block_count = document.isArray() ? document.array().size() : 0;

  spdlog::info(
      "editor snapshot received: bytes={}, blocks={}",
      snapshot_bytes.size(),
      block_count);

  return SuccessResponse(QVariant{});
}

}  // namespace cppwiki::bridge
