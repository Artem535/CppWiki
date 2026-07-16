#include "server/config/static_config_factory.h"

#include <rfl/Rename.hpp>
#include <rfl/yaml/write.hpp>

#include <optional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppwiki::server::config {

namespace {

constexpr std::string_view kAuthCheckerType = "cppwiki-auth-checker";

struct LoggerDefaultConfig final {
  rfl::Rename<"file_path", std::string> file_path{"@null"};
  std::string level;
  rfl::Rename<"overflow_behavior", std::string> overflow_behavior{"discard"};
};

struct LoggingConfig final {
  rfl::Rename<"fs-task-processor", std::string> fs_task_processor{"fs-task-processor"};

  struct Loggers final {
    rfl::Rename<"default", LoggerDefaultConfig> default_logger;
  } loggers;
};

struct ConnectionConfig final {
  rfl::Rename<"in_buffer_size", int> in_buffer_size{32768};
};

struct ListenerConfig final {
  std::uint16_t port;
  std::string address;
  rfl::Rename<"task_processor", std::string> task_processor{"main-task-processor"};
  ConnectionConfig connection{};
};

struct ServerConfig final {
  ListenerConfig listener;
};

struct HandlerAuthConfig final {
  std::vector<std::string> types;
  std::string issuer;
  std::string audience;
  rfl::Rename<"jwks_url", std::string> jwks_url;
};

struct PublicHandlerConfig final {
  std::string path;
  std::string method;
  rfl::Rename<"task_processor", std::string> task_processor{"main-task-processor"};
};

struct ProtectedHandlerConfig final {
  std::string path;
  std::string method;
  rfl::Rename<"task_processor", std::string> task_processor{"main-task-processor"};
  HandlerAuthConfig auth;
};

struct SyncBootstrapComponentConfig final {
  bool enabled{false};
  rfl::Rename<"gateway_url", std::string> gateway_url;
  rfl::Rename<"database_name", std::string> database_name{"cppwiki"};
  rfl::Rename<"admin_url", std::string> admin_url;
  rfl::Rename<"role_channels", std::map<std::string, std::vector<std::string>>> role_channels;
  rfl::Rename<"group_channels", std::map<std::string, std::vector<std::string>>> group_channels;
};

struct AiConfigComponentConfig final {
  bool enabled{false};
  rfl::Rename<"base_url", std::string> base_url{"https://api.openai.com/v1/chat/completions"};
  rfl::Rename<"api_key", std::string> api_key;
  std::string model;
};

struct HttpClientMiddlewarePipelineConfig final {
  struct Middlewares final {} middlewares{};
};

struct HttpClientConfig final {
  rfl::Rename<"core-component", std::string> core_component{"http-client-core"};
};

struct HttpClientCoreConfig final {
  rfl::Rename<"thread-name-prefix", std::string> thread_name_prefix{"http-client"};
  int threads{2};
  rfl::Rename<"fs-task-processor", std::string> fs_task_processor{"fs-task-processor"};
};

struct DnsClientConfig final {
  rfl::Rename<"fs-task-processor", std::string> fs_task_processor{"fs-task-processor"};
};

struct ComponentsConfig final {
  rfl::Rename<"http-client-middleware-pipeline", HttpClientMiddlewarePipelineConfig>
      http_client_middleware_pipeline{};
  rfl::Rename<"http-client", HttpClientConfig> http_client{};
  rfl::Rename<"http-client-core", HttpClientCoreConfig> http_client_core{};
  rfl::Rename<"dns-client", DnsClientConfig> dns_client{};
  LoggingConfig logging;
  ServerConfig server;
  rfl::Rename<"handler-health", PublicHandlerConfig> handler_health;
  rfl::Rename<"handler-options", PublicHandlerConfig> handler_options;
  rfl::Rename<"handler-openapi", std::optional<PublicHandlerConfig>> handler_openapi;
  rfl::Rename<"handler-swagger-ui", std::optional<PublicHandlerConfig>> handler_swagger_ui;
  rfl::Rename<"handler-locks", ProtectedHandlerConfig> handler_locks;
  rfl::Rename<"handler-presence", ProtectedHandlerConfig> handler_presence;
  rfl::Rename<"handler-sync-config", ProtectedHandlerConfig> handler_sync_config;
  rfl::Rename<"handler-admin-sync", ProtectedHandlerConfig> handler_admin_sync;
  rfl::Rename<"handler-workspaces", ProtectedHandlerConfig> handler_workspaces;
  rfl::Rename<"handler-protected-page", ProtectedHandlerConfig> handler_protected_page;
  rfl::Rename<"handler-ai-chat", ProtectedHandlerConfig> handler_ai_chat;
  rfl::Rename<"sync-config", SyncBootstrapComponentConfig> sync_config{};
  rfl::Rename<"ai-config", AiConfigComponentConfig> ai_config{};
};

struct MainTaskProcessorConfig final {
  rfl::Rename<"worker_threads", int> worker_threads{4};
  rfl::Rename<"thread_name", std::string> thread_name{"main-worker"};
};

struct TaskProcessorsConfig final {
  rfl::Rename<"main-task-processor", MainTaskProcessorConfig> main_task_processor{};
  rfl::Rename<"fs-task-processor", MainTaskProcessorConfig> fs_task_processor{
      MainTaskProcessorConfig{
          .worker_threads = 2,
          .thread_name = "fs-worker",
      }};
};

struct CoroPoolConfig final {
  rfl::Rename<"initial_size", int> initial_size{4};
  rfl::Rename<"max_size", int> max_size{128};
};

struct ComponentsManagerConfig final {
  rfl::Rename<"coro_pool", CoroPoolConfig> coro_pool{};
  rfl::Rename<"task_processors", TaskProcessorsConfig> task_processors{};
  rfl::Rename<"default_task_processor", std::string> default_task_processor{
      "main-task-processor"};
  ComponentsConfig components;
};

struct StaticConfig final {
  rfl::Rename<"components_manager", ComponentsManagerConfig> components_manager;
};

auto MakeLoggingConfig(const std::string& log_level) -> LoggingConfig {
  return LoggingConfig{
      .loggers =
          LoggingConfig::Loggers{
              .default_logger =
                  LoggerDefaultConfig{
                      .level = log_level,
                  },
          },
  };
}

auto MakeServerConfig(const std::string& host, std::uint16_t port) -> ServerConfig {
  return ServerConfig{
      .listener =
          ListenerConfig{
              .port = port,
              .address = host,
          },
  };
}

auto MakePublicHandler(std::string path, std::string method) -> PublicHandlerConfig {
  return PublicHandlerConfig{
      .path = std::move(path),
      .method = std::move(method),
  };
}

auto MakeProtectedHandler(std::string path, std::string method,
                          const ServerAuthConfig& auth_config) -> ProtectedHandlerConfig {
  return ProtectedHandlerConfig{
      .path = std::move(path),
      .method = std::move(method),
      .auth =
          HandlerAuthConfig{
              .types = {std::string(kAuthCheckerType)},
              .issuer = auth_config.issuer.value_or(""),
              .audience = auth_config.audience.value_or(""),
              .jwks_url = auth_config.jwks_url.value_or(""),
          },
  };
}

auto MakeSyncBootstrapConfig(const ServerSyncConfig& sync_config) -> SyncBootstrapComponentConfig {
  return SyncBootstrapComponentConfig{
      .enabled = sync_config.enabled,
      .gateway_url = sync_config.gateway_url.value_or(""),
      .database_name = sync_config.database_name.value_or("cppwiki"),
      .admin_url = sync_config.admin_url.value_or(""),
      .role_channels = sync_config.role_channels,
      .group_channels = sync_config.group_channels,
  };
}

auto MakeAiConfig(const ServerAiConfig& ai_config) -> AiConfigComponentConfig {
  AiConfigComponentConfig result;
  result.enabled = ai_config.enabled;
  if (ai_config.base_url) {
    result.base_url = *ai_config.base_url;
  }
  result.api_key = ai_config.api_key.value_or("");
  result.model = ai_config.model.value_or("");
  return result;
}

auto MakeStaticConfig(const std::string& host, std::uint16_t port, const std::string& log_level,
                      const ServerAuthConfig& auth_config, const ServerSyncConfig& sync_config,
                      const ServerAiConfig& ai_config, bool swagger_enabled) -> StaticConfig {
  ComponentsConfig components{
      .logging = MakeLoggingConfig(log_level),
      .server = MakeServerConfig(host, port),
      .handler_health = MakePublicHandler("/api/v1/health", "GET"),
      .handler_options = MakePublicHandler("/api/v1/health", "OPTIONS"),
      .handler_openapi =
          swagger_enabled ? std::make_optional(MakePublicHandler("/api/v1/openapi.json", "GET"))
                          : std::nullopt,
      .handler_swagger_ui =
          swagger_enabled ? std::make_optional(MakePublicHandler("/swagger/", "GET"))
                          : std::nullopt,
      .handler_locks =
          MakeProtectedHandler("/api/v1/locks/{document_id}", "GET,POST,PUT,DELETE", auth_config),
      .handler_presence =
          MakeProtectedHandler("/api/v1/presence/{workspace_id}", "GET,POST", auth_config),
      .handler_sync_config = MakeProtectedHandler("/api/v1/sync/config", "GET", auth_config),
      .handler_admin_sync = MakeProtectedHandler("/api/v1/admin/sync", "GET", auth_config),
      .handler_workspaces = MakeProtectedHandler("/api/v1/workspaces", "GET,POST", auth_config),
      .handler_protected_page = MakeProtectedHandler("/api/v1/protected", "GET", auth_config),
      .handler_ai_chat = MakeProtectedHandler("/api/v1/ai/chat", "POST", auth_config),
      .sync_config = MakeSyncBootstrapConfig(sync_config),
      .ai_config = MakeAiConfig(ai_config),
  };

  return StaticConfig{
      .components_manager =
          ComponentsManagerConfig{
              .components = std::move(components),
          },
  };
}

}  // namespace

auto MakeStaticConfigYaml(const std::string& host, std::uint16_t port, const std::string& log_level,
                          const ServerAuthConfig& auth_config,
                          const ServerSyncConfig& sync_config, const ServerAiConfig& ai_config,
                          bool swagger_enabled) -> std::string {
  return rfl::yaml::write(MakeStaticConfig(host, port, log_level, auth_config, sync_config,
                                           ai_config, swagger_enabled)) +
         "\n";
}

}  // namespace cppwiki::server::config
