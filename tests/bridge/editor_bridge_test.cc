#include "bridge/editor_bridge.h"

#include <spdlog/spdlog.h>

#include <QVariant>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "core/constants.h"
#include "core/qt_string.h"
#include "storage/local_document_repository.h"

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

class FakeDocumentRepository final : public cppwiki::storage::LocalDocumentRepository {
 public:
  [[nodiscard]] auto SaveDocument(const cppwiki::storage::DocumentRecord& document)
      -> cppwiki::storage::SaveDocumentResult override {
    documents_[document.metadata.id] = document;
    return cppwiki::storage::SaveDocumentResult{};
  }

  [[nodiscard]] auto LoadDocument(std::string_view page_id)
      -> cppwiki::storage::LoadDocumentResult override {
    const auto it = documents_.find(std::string(page_id));
    if (it == documents_.end()) {
      return cppwiki::storage::LoadDocumentResult{
          .document = std::nullopt,
          .error = cppwiki::storage::RepositoryError{
              .code = cppwiki::storage::RepositoryErrorCode::kReadFailed,
              .message = "Document was not found.",
          },
      };
    }

    return cppwiki::storage::LoadDocumentResult{
        .document = it->second,
        .error = std::nullopt,
    };
  }

  [[nodiscard]] auto ListDocuments() -> cppwiki::storage::ListDocumentsResult override {
    cppwiki::storage::ListDocumentsResult result;
    for (const auto& [id, document] : documents_) {
      result.documents.push_back(
          cppwiki::storage::DocumentSummaryFromMetadata(document.metadata));
    }
    return result;
  }

 private:
  std::map<std::string, cppwiki::storage::DocumentRecord> documents_;
};

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
  Require(methods.contains(cppwiki::ToQString(cppwiki::constants::kBridgeMethodListDocuments)),
          "missing list documents method");
  Require(methods.contains(cppwiki::ToQString(cppwiki::constants::kBridgeMethodCreateDocument)),
          "missing create document method");
  Require(methods.contains(cppwiki::ToQString(cppwiki::constants::kBridgeMethodLoadDocument)),
          "missing load document method");
  Require(methods.contains(cppwiki::ToQString(cppwiki::constants::kBridgeMethodOpenDocument)),
          "missing open document method");
  Require(methods.contains(cppwiki::ToQString(cppwiki::constants::kBridgeMethodUpdateSnapshot)),
          "missing update snapshot method");
}

auto TestInitialDocumentStartsEmpty() -> void {
  cppwiki::bridge::QEditorBridge bridge;
  const auto response = bridge.getInitialDocument();

  RequireSuccessEnvelope(response);

  const auto blocks = response.value(QStringLiteral("result")).toList();
  Require(blocks.empty(), "initial document should be empty until a page is selected");
}

auto TestDocumentListBootstrapsWelcomePage() -> QString {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto response = bridge.listDocuments();
  RequireSuccessEnvelope(response);

  const auto pages = response.value(QStringLiteral("result")).toList();
  Require(pages.size() == 1, "empty repository should be bootstrapped with one page");

  const auto page = pages.front().toMap();
  Require(page.value(QStringLiteral("title")).toString() == QStringLiteral("Welcome to CppWiki"),
          "bootstrap page title should match");
  return page.value(QStringLiteral("id")).toString();
}

auto TestCreateDocument() -> QString {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto response = bridge.createDocument();
  RequireSuccessEnvelope(response);

  const auto created = response.value(QStringLiteral("result")).toMap();
  Require(created.value(QStringLiteral("title")).toString() == QStringLiteral("Untitled note"),
          "created document title should match");
  Require(!created.value(QStringLiteral("id")).toString().isEmpty(),
          "created document id should not be empty");
  return created.value(QStringLiteral("id")).toString();
}

auto TestOpenDocumentReturnsLoadedDocument() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);

  const auto list_response = bridge.listDocuments();
  RequireSuccessEnvelope(list_response);
  const auto page_id = list_response.value(QStringLiteral("result")).toList().front().toMap().value(
      QStringLiteral("id")).toString();

  const auto response = bridge.openDocument(page_id);
  RequireSuccessEnvelope(response);

  const auto document = response.value(QStringLiteral("result")).toMap();
  Require(document.value(QStringLiteral("id")).toString() == page_id,
          "opened document id should match selected page");
  Require(document.value(QStringLiteral("blocks")).toList().size() == 2,
          "opened welcome document should include blocks");
}

auto TestValidSnapshot() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);
  const auto list_response = bridge.listDocuments();
  RequireSuccessEnvelope(list_response);
  const auto page_id = list_response.value(QStringLiteral("result")).toList().front().toMap().value(
      QStringLiteral("id")).toString();

  RequireSuccessEnvelope(bridge.loadDocument(page_id));

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
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);
  const auto list_response = bridge.listDocuments();
  RequireSuccessEnvelope(list_response);
  const auto page_id = list_response.value(QStringLiteral("result")).toList().front().toMap().value(
      QStringLiteral("id")).toString();
  RequireSuccessEnvelope(bridge.loadDocument(page_id));

  const auto response = bridge.updateSnapshot(QStringLiteral("{"));

  RequireErrorEnvelope(response, QStringLiteral("invalid_json"));
}

auto TestInvalidRootSnapshot() -> void {
  auto repository = std::make_shared<FakeDocumentRepository>();
  cppwiki::bridge::QEditorBridge bridge;
  bridge.SetRepository(repository);
  const auto list_response = bridge.listDocuments();
  RequireSuccessEnvelope(list_response);
  const auto page_id = list_response.value(QStringLiteral("result")).toList().front().toMap().value(
      QStringLiteral("id")).toString();
  RequireSuccessEnvelope(bridge.loadDocument(page_id));

  const auto response = bridge.updateSnapshot(QStringLiteral(R"({ "type": "paragraph" })"));

  RequireErrorEnvelope(response, QStringLiteral("missing_schema_version"));
}

}  // namespace

auto main() -> int {
  TestBridgeInfo();
  TestInitialDocumentStartsEmpty();
  TestDocumentListBootstrapsWelcomePage();
  TestCreateDocument();
  TestOpenDocumentReturnsLoadedDocument();
  TestValidSnapshot();
  TestInvalidJsonSnapshot();
  TestInvalidRootSnapshot();

  spdlog::info("cppwiki_bridge_tests passed");
  return EXIT_SUCCESS;
}
