#include "server/config/static_config_factory.h"

#include <rfl/Rename.hpp>
#include <rfl/yaml/write.hpp>

#include <optional>
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

struct ComponentsConfig final {
  LoggingConfig logging;
  ServerConfig server;
  rfl::Rename<"handler-health", PublicHandlerConfig> handler_health;
  rfl::Rename<"handler-options", PublicHandlerConfig> handler_options;
  rfl::Rename<"handler-openapi", PublicHandlerConfig> handler_openapi;
  rfl::Rename<"handler-locks", ProtectedHandlerConfig> handler_locks;
  rfl::Rename<"handler-presence", ProtectedHandlerConfig> handler_presence;
  rfl::Rename<"handler-protected-page", ProtectedHandlerConfig> handler_protected_page;
  std::optional<rfl::Rename<"handler-swagger-ui", PublicHandlerConfig>> handler_swagger_ui{
      std::nullopt};
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

auto MakeProtectedHandler(std::string path, std::string method) -> ProtectedHandlerConfig {
  return ProtectedHandlerConfig{
      .path = std::move(path),
      .method = std::move(method),
      .auth =
          HandlerAuthConfig{
              .types = {std::string(kAuthCheckerType)},
          },
  };
}

auto MakeStaticConfig(const std::string& host, std::uint16_t port, const std::string& log_level,
                      bool swagger_enabled) -> StaticConfig {
  ComponentsConfig components{
      .logging = MakeLoggingConfig(log_level),
      .server = MakeServerConfig(host, port),
      .handler_health = MakePublicHandler("/api/v1/health", "GET"),
      .handler_options = MakePublicHandler("/api/v1/health", "OPTIONS"),
      .handler_openapi = MakePublicHandler("/api/v1/openapi.json", "GET"),
      .handler_locks = MakeProtectedHandler("/api/v1/locks/{document_id}", "GET,POST,PUT,DELETE"),
      .handler_presence = MakeProtectedHandler("/api/v1/presence/{workspace_id}", "GET,POST"),
      .handler_protected_page = MakeProtectedHandler("/api/v1/protected", "GET"),
  };

  if (swagger_enabled) {
    components.handler_swagger_ui = rfl::Rename<"handler-swagger-ui", PublicHandlerConfig>{
        MakePublicHandler("/swagger/", "GET")};
  }

  return StaticConfig{
      .components_manager =
          ComponentsManagerConfig{
              .components = std::move(components),
          },
  };
}

}  // namespace

auto MakeStaticConfigYaml(const std::string& host, std::uint16_t port, const std::string& log_level,
                          bool swagger_enabled) -> std::string {
  return rfl::yaml::write(MakeStaticConfig(host, port, log_level, swagger_enabled)) + "\n";
}

}  // namespace cppwiki::server::config
