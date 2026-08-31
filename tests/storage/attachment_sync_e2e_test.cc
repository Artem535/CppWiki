#include <spdlog/spdlog.h>

#include <cbl++/CouchbaseLite.hh>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "core/constants.h"
#include "storage/attachment.h"
#include "storage/cblite_document_repository.h"
#include "sync/sync_bootstrap.h"

namespace {

constexpr std::string_view kGatewayUrlEnvironmentVariable = "CPPWIKI_ATTACHMENT_E2E_GATEWAY_URL";
constexpr std::string_view kDatabaseName = "cppwiki";
constexpr std::string_view kWorkspaceId = "engineering";

auto Slice(std::string_view value) -> cbl::slice {
  return cbl::slice(value.data(), value.size());
}

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto Replicate(cbl::Collection collection, std::string_view gateway_url,
               CBLReplicatorType direction) -> void {
  cbl::ReplicationCollection collection_config(std::move(collection));
  collection_config.channels.append(std::string("workspace:") + std::string(kWorkspaceId));

  std::vector<cbl::ReplicationCollection> collections;
  collections.push_back(std::move(collection_config));
  auto endpoint = cbl::Endpoint::urlEndpoint(Slice(gateway_url));
  cbl::ReplicatorConfiguration config(std::move(collections), endpoint);
  config.replicatorType = direction;
  config.maxAttempts = 1;

  cbl::Replicator replicator(config);
  replicator.start();

  constexpr auto kTimeout = std::chrono::seconds(30);
  const auto deadline = std::chrono::steady_clock::now() + kTimeout;
  while (std::chrono::steady_clock::now() < deadline) {
    const auto status = replicator.status();
    if (status.activity == kCBLReplicatorStopped) {
      Require(status.error.code == 0, "Couchbase Lite replication must finish without an error");
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  replicator.stop();
  Require(false, "Couchbase Lite replication must finish before its timeout");
}

auto MakeBootstrap() -> cppwiki::sync::SyncBootstrap {
  cppwiki::sync::SyncBootstrap bootstrap;
  bootstrap.available = true;
  bootstrap.enabled = true;
  bootstrap.gateway_url = QString::fromUtf8(std::getenv(kGatewayUrlEnvironmentVariable.data()));
  bootstrap.database_name =
      QString::fromUtf8(kDatabaseName.data(), static_cast<qsizetype>(kDatabaseName.size()));
  bootstrap.auth_mode = QStringLiteral("oidc_access_token_passthrough");
  bootstrap.token_passthrough = true;
  bootstrap.channels = {QStringLiteral("workspace:engineering")};
  return bootstrap;
}

auto OpenDatabase(std::string_view database_name, const std::filesystem::path& directory)
    -> cbl::Database {
  auto config = CBLDatabaseConfiguration_Default();
  const auto directory_string = directory.string();
  config.directory = Slice(directory_string);
  return cbl::Database(Slice(database_name), config);
}

auto TestAttachmentSync() -> void {
  const char* const gateway_url = std::getenv(kGatewayUrlEnvironmentVariable.data());
  Require(gateway_url != nullptr && std::string_view(gateway_url).starts_with("ws://"),
          "CPPWIKI_ATTACHMENT_E2E_GATEWAY_URL must contain a ws:// Sync Gateway URL");

  const auto test_root = std::filesystem::temp_directory_path() / "cppwiki-attachment-sync-e2e";
  const auto source_directory = test_root / "source";
  const auto target_directory = test_root / "target";
  std::filesystem::remove_all(test_root);
  std::filesystem::create_directories(source_directory);
  std::filesystem::create_directories(target_directory);

  const cppwiki::storage::AttachmentData attachment{
      .metadata =
          cppwiki::storage::AttachmentMetadata{
              .id = "f0caa6fc-2e82-4669-8557-0adf7751d2cc",
              .workspace_id = std::string(kWorkspaceId),
              .filename = "architecture.png",
              .mime_type = "image/png",
              .size_bytes = 8,
              .sha256 = "0123456789abcdef",
              .created_at = "2026-08-31T12:00:00Z",
              .created_by = "attachment-sync-e2e",
          },
      .bytes = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A},
  };

  {
    cppwiki::storage::CbliteDocumentRepository source_repository({
        .database_directory = source_directory,
        .database_name = "source",
    });
    Require(!source_repository.ApplySyncBootstrap(MakeBootstrap()).error,
            "source repository must configure the synced workspace");
    Require(!source_repository.SaveAttachment(attachment).error,
            "source repository must save the attachment to the synced collection");
  }

  try {
    {
      auto source_database = OpenDatabase("source", source_directory);
      auto source_collection =
          source_database.getCollection(Slice(cppwiki::constants::kDocumentsCollectionName));
      Require(static_cast<bool>(source_collection), "source documents collection must exist");
      Replicate(std::move(source_collection), gateway_url, kCBLReplicatorTypePush);
      source_database.close();
    }

    {
      auto target_database = OpenDatabase("target", target_directory);
      auto target_collection =
          target_database.createCollection(Slice(cppwiki::constants::kDocumentsCollectionName));
      Replicate(std::move(target_collection), gateway_url, kCBLReplicatorTypePull);
      target_database.close();
    }
  } catch (const CBLError& error) {
    spdlog::error("FAIL: Couchbase Lite replication error domain={} code={}",
                  static_cast<int>(error.domain), error.code);
    std::filesystem::remove_all(test_root);
    std::exit(EXIT_FAILURE);
  } catch (const std::exception& error) {
    spdlog::error("FAIL: {}", error.what());
    std::filesystem::remove_all(test_root);
    std::exit(EXIT_FAILURE);
  }

  cppwiki::storage::CbliteDocumentRepository target_repository({
      .database_directory = target_directory,
      .database_name = "target",
  });
  const auto loaded =
      target_repository.LoadAttachment(attachment.metadata.id, attachment.metadata.workspace_id);
  Require(!loaded.error && loaded.attachment.has_value(),
          "target repository must load the attachment received through Sync Gateway");
  Require(loaded.attachment->metadata.filename == attachment.metadata.filename,
          "target attachment metadata must survive replication");
  Require(loaded.attachment->bytes == attachment.bytes,
          "target attachment Blob bytes must survive replication");

  std::filesystem::remove_all(test_root);
}

}  // namespace

auto main() -> int {
  TestAttachmentSync();
  spdlog::info("cppwiki_attachment_sync_e2e_tests passed");
  return EXIT_SUCCESS;
}
