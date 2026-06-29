#include "sync/document_sync_service.h"

#include <spdlog/spdlog.h>

#include <QCoreApplication>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>

#include "app/program_settings.h"
#include "core/constants.h"
#include "core/qt_string.h"
#include "storage/local_document_repository.h"

#ifdef CPPWIKI_ENABLE_CBLITE_STORAGE
#include "storage/cblite_document_repository.h"
#endif

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

class FakeDocumentRepository final : public cppwiki::storage::LocalDocumentRepository {
 public:
  [[nodiscard]] auto SaveDocument(const cppwiki::storage::DocumentRecord&)
      -> cppwiki::storage::SaveDocumentResult override {
    return {};
  }

  [[nodiscard]] auto DeleteDocument(std::string_view) -> cppwiki::storage::DeleteDocumentResult override {
    return {};
  }

  [[nodiscard]] auto LoadDocument(std::string_view) -> cppwiki::storage::LoadDocumentResult override {
    return {};
  }

  [[nodiscard]] auto ListDocuments() -> cppwiki::storage::ListDocumentsResult override {
    return {};
  }

  [[nodiscard]] auto SupportsSync() const -> bool override { return true; }

  [[nodiscard]] auto SetSyncAccessToken(std::string access_token)
      -> cppwiki::storage::SyncOperationResult override {
    access_token_set = true;
    last_access_token = std::move(access_token);
    return {};
  }

  [[nodiscard]] auto ApplySyncBootstrap(const cppwiki::sync::SyncBootstrap& bootstrap)
      -> cppwiki::storage::SyncOperationResult override {
    bootstrap_applied = true;
    last_bootstrap = bootstrap;
    sync_status = {
        .state = cppwiki::storage::SyncLifecycleState::kConfigured,
        .status_text = "Sync bootstrap configured",
    };
    return {};
  }

  [[nodiscard]] auto StartSync() -> cppwiki::storage::SyncOperationResult override {
    start_sync_called = true;
    sync_status = {
        .state = cppwiki::storage::SyncLifecycleState::kRunning,
        .status_text = "Sync running",
    };
    return {};
  }

  [[nodiscard]] auto StopSync() -> cppwiki::storage::SyncOperationResult override {
    stop_sync_called = true;
    sync_status = {
        .state = cppwiki::storage::SyncLifecycleState::kConfigured,
        .status_text = "Sync stopped",
    };
    return {};
  }

  [[nodiscard]] auto GetSyncStatus() const -> cppwiki::storage::SyncStatus override {
    return sync_status;
  }

  bool bootstrap_applied = false;
  bool start_sync_called = false;
  bool stop_sync_called = false;
  bool access_token_set = false;
  cppwiki::sync::SyncBootstrap last_bootstrap;
  std::string last_access_token;
  cppwiki::storage::SyncStatus sync_status{};
};

auto MakeSettings() -> cppwiki::ProgramSettings {
  return cppwiki::ProgramSettings(
      cppwiki::ToQString(cppwiki::constants::kApplicationName),
      cppwiki::ToQString(cppwiki::constants::kApplicationVersion),
      cppwiki::ToQString(cppwiki::constants::kOrganizationName), QStringLiteral("/tmp/cppwiki-app"),
      QStringLiteral("/tmp/cppwiki-db"), QStringLiteral("/tmp/cppwiki-editor"),
      QStringLiteral("http://127.0.0.1:8080"), true,
      QStringLiteral("http://127.0.0.1:9000/application/o/authorize/"),
      QStringLiteral("http://127.0.0.1:9000/application/o/token/"),
      QStringLiteral("cppwiki-desktop"), QStringLiteral("http://127.0.0.1:38080/auth/callback"),
      true, false, {}, true, 12);
}

auto MakeBaseBootstrap() -> cppwiki::sync::SyncBootstrap {
  cppwiki::sync::SyncBootstrap bootstrap;
  bootstrap.available = true;
  bootstrap.enabled = true;
  bootstrap.gateway_url = QStringLiteral("http://127.0.0.1:4984/cppwiki");
  bootstrap.database_name = QStringLiteral("cppwiki");
  bootstrap.auth_mode = QStringLiteral("oidc_access_token_passthrough");
  bootstrap.token_passthrough = true;
  bootstrap.principal_subject = QStringLiteral("subject-1");
  bootstrap.principal_username = QStringLiteral("akadmin");
  bootstrap.principal_email = QStringLiteral("mail@example.com");
  bootstrap.channels = {QStringLiteral("user:subject-1")};
  bootstrap.status_text = QStringLiteral("Sync bootstrap is ready");
  return bootstrap;
}

auto TestMissingChannelsIsRejected() -> void {
  cppwiki::sync::DocumentSyncService service;
  service.ApplySettings(MakeSettings());
  auto repository = std::make_shared<FakeDocumentRepository>();
  service.SetRepository(repository);
  service.SetAccessToken(QStringLiteral("test-token"));

  auto bootstrap = MakeBaseBootstrap();
  bootstrap.channels.clear();
  service.SetBackendBootstrap(std::move(bootstrap));

  Require(service.State() == cppwiki::sync::DocumentSyncState::kUnavailable,
          "sync service should reject bootstrap without channels");
  Require(service.StatusText().contains(QStringLiteral("did not assign sync channels")),
          "sync service should explain missing channels");
  Require(!repository->bootstrap_applied,
          "sync service should not configure repository when bootstrap is invalid");
}

auto TestMissingTokenPassthroughIsRejected() -> void {
  cppwiki::sync::DocumentSyncService service;
  service.ApplySettings(MakeSettings());
  auto repository = std::make_shared<FakeDocumentRepository>();
  service.SetRepository(repository);
  service.SetAccessToken(QStringLiteral("test-token"));

  auto bootstrap = MakeBaseBootstrap();
  bootstrap.token_passthrough = false;
  service.SetBackendBootstrap(std::move(bootstrap));

  Require(service.State() == cppwiki::sync::DocumentSyncState::kUnavailable,
          "sync service should reject bootstrap without token passthrough");
  Require(service.StatusText().contains(QStringLiteral("does not allow token passthrough")),
          "sync service should explain missing token passthrough");
  Require(!repository->bootstrap_applied,
          "sync service should not configure repository without token passthrough");
}

auto TestValidBootstrapConfiguresRepositoryLifecycle() -> void {
  cppwiki::sync::DocumentSyncService service;
  service.ApplySettings(MakeSettings());
  auto repository = std::make_shared<FakeDocumentRepository>();
  service.SetRepository(repository);
  service.SetAccessToken(QStringLiteral("test-token"));
  service.SetBackendBootstrap(MakeBaseBootstrap());

  Require(service.State() == cppwiki::sync::DocumentSyncState::kReady,
          "sync service should become ready with valid bootstrap");
  Require(repository->bootstrap_applied,
          "sync service should apply bootstrap to sync-capable repository");
  Require(repository->access_token_set,
          "sync service should pass access token to sync-capable repository");
  Require(repository->last_access_token == "test-token",
          "repository should receive current access token");
  Require(repository->start_sync_called,
          "sync service should start repository sync lifecycle");
  Require(repository->last_bootstrap.gateway_url == QStringLiteral("http://127.0.0.1:4984/cppwiki"),
          "repository should receive backend bootstrap");
}

#ifdef CPPWIKI_ENABLE_CBLITE_STORAGE
auto TestReadyStateIncludesPrincipalAndChannels() -> void {
  const auto test_directory =
      std::filesystem::temp_directory_path() / "cppwiki-sync-service-test" / "database";
  std::filesystem::remove_all(test_directory.parent_path());

  auto repository = std::make_shared<cppwiki::storage::CbliteDocumentRepository>(
      cppwiki::storage::CbliteDocumentRepositoryOptions{
          .database_directory = test_directory,
          .database_name = "cppwiki",
      });

  cppwiki::sync::DocumentSyncService service;
  service.ApplySettings(MakeSettings());
  service.SetRepository(std::move(repository));
  service.SetAccessToken(QStringLiteral("test-token"));

  auto bootstrap = MakeBaseBootstrap();
  bootstrap.channels = {QStringLiteral("user:subject-1"), QStringLiteral("workspace:default")};
  service.SetBackendBootstrap(std::move(bootstrap));

  Require(service.State() == cppwiki::sync::DocumentSyncState::kReady,
          "sync service should become ready with valid bootstrap and cblite repository");
  Require(service.StatusText().contains(QStringLiteral("akadmin")),
          "ready text should include principal username");
  Require(service.StatusText().contains(QStringLiteral("2 channels")),
          "ready text should include assigned channel count");

  std::filesystem::remove_all(test_directory.parent_path());
}
#endif

}  // namespace

auto main(int argc, char* argv[]) -> int {
  QCoreApplication application(argc, argv);
  QCoreApplication::setApplicationName(cppwiki::ToQString(cppwiki::constants::kApplicationName));
  QCoreApplication::setApplicationVersion(
      cppwiki::ToQString(cppwiki::constants::kApplicationVersion));
  QCoreApplication::setOrganizationName(cppwiki::ToQString(cppwiki::constants::kOrganizationName));

  TestMissingChannelsIsRejected();
  TestMissingTokenPassthroughIsRejected();
  TestValidBootstrapConfiguresRepositoryLifecycle();
#ifdef CPPWIKI_ENABLE_CBLITE_STORAGE
  TestReadyStateIncludesPrincipalAndChannels();
#endif

  spdlog::info("cppwiki_document_sync_service_tests passed");
  return EXIT_SUCCESS;
}
