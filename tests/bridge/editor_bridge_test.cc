#include "bridge/editor_bridge.h"

#include <spdlog/spdlog.h>

#include <QVariant>
#include <cstdlib>
#include <string_view>

#include "core/constants.h"
#include "core/qt_string.h"

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto RequireSuccessEnvelope(const QVariantMap& response) -> void {
  Require(
      response.value(QStringLiteral("apiVersion")).toInt() == cppwiki::constants::kBridgeApiVersion,
      "response apiVersion must be 1");
  Require(response.value(QStringLiteral("ok")).toBool(), "response must be ok");
  Require(response.contains(QStringLiteral("result")), "result must exist");
}

auto RequireErrorEnvelope(const QVariantMap& response, const QString& expected_code) -> void {
  Require(
      response.value(QStringLiteral("apiVersion")).toInt() == cppwiki::constants::kBridgeApiVersion,
      "error response apiVersion must be 1");
  Require(!response.value(QStringLiteral("ok")).toBool(), "error response must not be ok");

  const auto error = response.value(QStringLiteral("error")).toMap();
  Require(error.value(QStringLiteral("code")).toString() == expected_code, "error code mismatch");
  Require(!error.value(QStringLiteral("message")).toString().isEmpty(),
          "error message must not be empty");
}

auto TestBridgeInfo() -> void {
  cppwiki::bridge::QEditorBridge bridge;
  const auto response = bridge.getBridgeInfo();

  RequireSuccessEnvelope(response);

  const auto result = response.value(QStringLiteral("result")).toMap();
  Require(
      result.value(QStringLiteral("apiVersion")).toInt() == cppwiki::constants::kBridgeApiVersion,
      "bridge info apiVersion must be 1");
  Require(result.value(QStringLiteral("namespace")).toString() ==
              cppwiki::ToQString(cppwiki::constants::kDocumentsBridgeNamespace),
          "bridge namespace mismatch");

  const auto methods = result.value(QStringLiteral("methods")).toList();
  Require(methods.contains(cppwiki::ToQString(cppwiki::constants::kBridgeMethodGetBridgeInfo)),
          "missing info method");
  Require(methods.contains(cppwiki::ToQString(cppwiki::constants::kBridgeMethodGetInitialDocument)),
          "missing initial document method");
  Require(methods.contains(cppwiki::ToQString(cppwiki::constants::kBridgeMethodUpdateSnapshot)),
          "missing update snapshot method");
}

auto TestInitialDocument() -> void {
  cppwiki::bridge::QEditorBridge bridge;
  const auto response = bridge.getInitialDocument();

  RequireSuccessEnvelope(response);

  const auto blocks = response.value(QStringLiteral("result")).toList();
  Require(blocks.size() == 2, "initial document should have two blocks");

  const auto heading = blocks.front().toMap();
  Require(heading.value(QStringLiteral("type")).toString() == QStringLiteral("heading"),
          "first initial block should be a heading");
}

auto TestValidSnapshot() -> void {
  cppwiki::bridge::QEditorBridge bridge;
  const auto response = bridge.updateSnapshot(QStringLiteral(R"([
    {
      "id": "b1",
      "type": "paragraph",
      "content": [
        { "type": "text", "text": "Saved from test", "styles": {} }
      ],
      "children": []
    },
    {
      "id": "quote-1",
      "type": "quote",
      "content": [
        { "type": "text", "text": "Quoted from bridge test", "styles": {} }
      ],
      "children": []
    }
  ])"));

  RequireSuccessEnvelope(response);
}

auto TestInvalidJsonSnapshot() -> void {
  cppwiki::bridge::QEditorBridge bridge;
  const auto response = bridge.updateSnapshot(QStringLiteral("{"));

  RequireErrorEnvelope(response, QStringLiteral("invalid_json"));
}

auto TestInvalidRootSnapshot() -> void {
  cppwiki::bridge::QEditorBridge bridge;
  const auto response = bridge.updateSnapshot(QStringLiteral(R"({ "type": "paragraph" })"));

  RequireErrorEnvelope(response, QStringLiteral("missing_schema_version"));
}

}  // namespace

auto main() -> int {
  TestBridgeInfo();
  TestInitialDocument();
  TestValidSnapshot();
  TestInvalidJsonSnapshot();
  TestInvalidRootSnapshot();

  spdlog::info("cppwiki_bridge_tests passed");
  return EXIT_SUCCESS;
}
