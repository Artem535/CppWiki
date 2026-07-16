#include "admin/admin_response_parsing.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string>
#include <string_view>

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto TestNormalizeBaseUrl() -> void {
  Require(cppwiki::admin::NormalizeBaseUrl("http://host:8080/") == "http://host:8080",
          "NormalizeBaseUrl should strip a single trailing slash");
  Require(cppwiki::admin::NormalizeBaseUrl("http://host:8080///") == "http://host:8080",
          "NormalizeBaseUrl should strip multiple trailing slashes");
  Require(cppwiki::admin::NormalizeBaseUrl("http://host:8080") == "http://host:8080",
          "NormalizeBaseUrl should leave a URL with no trailing slash unchanged");
}

auto TestBuildApiUrl() -> void {
  Require(cppwiki::admin::BuildApiUrl("http://host:8080/", "/api/v1/workspaces") ==
              "http://host:8080/api/v1/workspaces",
          "BuildApiUrl should join base url and path without a double slash");
}

auto TestBuildWorkspaceCreateBody() -> void {
  const auto body = cppwiki::admin::BuildWorkspaceCreateBody("team-alpha");
  Require(body.find("team-alpha") != std::string::npos,
          "BuildWorkspaceCreateBody should embed the workspace id");
  Require(body.find("\"id\"") != std::string::npos,
          "BuildWorkspaceCreateBody should use the 'id' field name expected by workspace_handler");
}

auto TestExtractEnvelopeErrorFromErrorEnvelope() -> void {
  const std::string body =
      R"({"apiVersion":1,"ok":false,"error":{"code":"forbidden","message":"Only workspace admins can create workspaces"}})";
  const auto message = cppwiki::admin::ExtractEnvelopeError(body);
  Require(message.has_value(), "ExtractEnvelopeError should find an error message");
  Require(*message == "Only workspace admins can create workspaces",
          "ExtractEnvelopeError should return the server-provided message");
}

auto TestExtractEnvelopeErrorFromNonErrorBody() -> void {
  const std::string body = R"({"apiVersion":1,"ok":true,"result":{"workspaces":[]}})";
  const auto message = cppwiki::admin::ExtractEnvelopeError(body);
  Require(!message.has_value(),
          "ExtractEnvelopeError should return nullopt for a success envelope");
}

auto TestParseWorkspaceIds() -> void {
  const std::string body =
      R"({"apiVersion":1,"ok":true,"result":{"workspaces":[{"id":"team-alpha"},{"id":"team-beta"}]}})";
  const auto ids = cppwiki::admin::ParseWorkspaceIds(body);
  Require(ids.size() == 2, "ParseWorkspaceIds should return both workspace ids");
  Require(ids[0] == "team-alpha" && ids[1] == "team-beta",
          "ParseWorkspaceIds should preserve server-provided order");
}

auto TestParseWorkspaceIdsOnErrorEnvelope() -> void {
  const std::string body =
      R"({"apiVersion":1,"ok":false,"error":{"code":"forbidden","message":"nope"}})";
  const auto ids = cppwiki::admin::ParseWorkspaceIds(body);
  Require(ids.empty(), "ParseWorkspaceIds should return an empty list for an error envelope");
}

auto TestParseWorkspaceCreateResult() -> void {
  const std::string body =
      R"({"apiVersion":1,"ok":true,"result":{"workspace":{"id":"team-gamma"},"created":true}})";
  const auto result = cppwiki::admin::ParseWorkspaceCreateResult(body);
  Require(result.has_value(), "ParseWorkspaceCreateResult should parse a success envelope");
  Require(result->id == "team-gamma", "ParseWorkspaceCreateResult should extract the workspace id");
  Require(result->created, "ParseWorkspaceCreateResult should extract the created flag");
}

auto TestParseAdminSyncOverview() -> void {
  const std::string body = R"({
    "apiVersion":1,
    "ok":true,
    "result":{
      "available":true,
      "enabled":true,
      "gatewayUrl":"http://sync-gateway:4984",
      "adminUrl":"http://sync-gateway:4985",
      "databaseName":"cppwiki",
      "statusText":"Sync: reachable",
      "workspaces":["team-alpha"],
      "roleChannels":{"wiki.admin":["admin-channel"]},
      "groupChannels":{}
    }
  })";
  const auto overview = cppwiki::admin::ParseAdminSyncOverview(body);
  Require(overview.has_value(), "ParseAdminSyncOverview should parse a success envelope");
  Require(overview->available && overview->enabled,
          "ParseAdminSyncOverview should extract available/enabled flags");
  Require(overview->gateway_url == "http://sync-gateway:4984",
          "ParseAdminSyncOverview should extract the gateway url");
  Require(overview->workspaces.size() == 1 && overview->workspaces[0] == "team-alpha",
          "ParseAdminSyncOverview should extract workspace ids");
  Require(overview->role_channels.at("wiki.admin").size() == 1,
          "ParseAdminSyncOverview should extract role channel mappings");
}

auto TestIsHttpSuccessStatus() -> void {
  Require(cppwiki::admin::IsHttpSuccessStatus(200), "200 should be a success status");
  Require(cppwiki::admin::IsHttpSuccessStatus(201), "201 should be a success status");
  Require(!cppwiki::admin::IsHttpSuccessStatus(401), "401 should not be a success status");
  Require(!cppwiki::admin::IsHttpSuccessStatus(500), "500 should not be a success status");
}

}  // namespace

auto main() -> int {
  TestNormalizeBaseUrl();
  TestBuildApiUrl();
  TestBuildWorkspaceCreateBody();
  TestExtractEnvelopeErrorFromErrorEnvelope();
  TestExtractEnvelopeErrorFromNonErrorBody();
  TestParseWorkspaceIds();
  TestParseWorkspaceIdsOnErrorEnvelope();
  TestParseWorkspaceCreateResult();
  TestParseAdminSyncOverview();
  TestIsHttpSuccessStatus();

  spdlog::info("cppwiki_admin_cli_tests passed");
  return EXIT_SUCCESS;
}
