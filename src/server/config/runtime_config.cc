#include "server/config/runtime_config.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <rfl/yaml/read.hpp>
#include <rfl/yaml/write.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "core/constants.h"

namespace cppwiki::server::config {

namespace {

struct RuntimeConfigFile final {
  std::optional<std::string> bind_host;
  std::optional<std::uint16_t> port;
  std::optional<std::string> log_level;
};

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
  rfl::Rename<"handler-locks", ProtectedHandlerConfig> handler_locks;
  rfl::Rename<"handler-presence", ProtectedHandlerConfig> handler_presence;
  rfl::Rename<"handler-protected-page", ProtectedHandlerConfig> handler_protected_page;
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

auto Normalize(std::string_view value) -> std::string {
  std::string out(value);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

auto ReadFile(const std::string& path) -> std::string {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Could not open server config: " + path);
  }
  std::ostringstream ss;
  ss << input.rdbuf();
  return ss.str();
}

auto ConvertToUint16(const std::string& value) -> std::uint16_t {
  const long parsed = std::strtol(value.c_str(), nullptr, 10);
  if (parsed <= 0 || parsed > 65535) {
    throw std::invalid_argument("Invalid port value: " + value);
  }
  return static_cast<std::uint16_t>(parsed);
}

auto ConvertLevel(const std::string& level) -> std::string {
  if (const auto normalized = Normalize(level); normalized == "trace" || normalized == "debug" ||
                                                normalized == "info" || normalized == "warn" ||
                                                normalized == "error" || normalized == "critical" ||
                                                normalized == "off" || normalized == "warning") {
    return normalized;
  }
  throw std::invalid_argument("Unsupported log level: " + level);
}

auto LoadRuntimeConfigFile(const std::string& path) -> RuntimeConfigFile {
  const auto yaml = ReadFile(path);
  auto parsed = rfl::yaml::read<RuntimeConfigFile>(yaml);
  if (!parsed) {
    throw std::runtime_error("Could not parse server config: " + path + ": " +
                             parsed.error().what());
  }

  auto file_config = parsed.value();
  if (file_config.port) {
    file_config.port = ConvertToUint16(std::to_string(*file_config.port));
  }
  if (file_config.log_level) {
    file_config.log_level = ConvertLevel(*file_config.log_level);
  }

  return file_config;
}

auto MakeStaticConfig(const std::string& host, const std::uint16_t port,
                      const std::string& log_level) -> StaticConfig {
  return StaticConfig{
      .components_manager =
          ComponentsManagerConfig{
              .components =
                  ComponentsConfig{
                      .logging =
                          LoggingConfig{
                              .loggers =
                                  LoggingConfig::Loggers{
                                      .default_logger =
                                          LoggerDefaultConfig{
                                              .level = log_level,
                                          },
                                  },
                          },
                      .server =
                          ServerConfig{
                              .listener =
                                  ListenerConfig{
                                      .port = port,
                                      .address = host,
                                  },
                          },
                      .handler_health =
                          PublicHandlerConfig{
                              .path = "/api/v1/health",
                              .method = "GET",
                          },
                      .handler_options =
                          PublicHandlerConfig{
                              .path = "/api/v1/health",
                              .method = "OPTIONS",
                          },
                      .handler_locks =
                          ProtectedHandlerConfig{
                              .path = "/api/v1/locks/{document_id}",
                              .method = "GET,POST,PUT,DELETE",
                              .auth =
                                  HandlerAuthConfig{
                                      .types = {"cppwiki-auth-checker"},
                                  },
                          },
                      .handler_presence =
                          ProtectedHandlerConfig{
                              .path = "/api/v1/presence/{workspace_id}",
                              .method = "GET,POST",
                              .auth =
                                  HandlerAuthConfig{
                                      .types = {"cppwiki-auth-checker"},
                                  },
                          },
                      .handler_protected_page =
                          ProtectedHandlerConfig{
                              .path = "/api/v1/protected",
                              .method = "GET",
                              .auth =
                                  HandlerAuthConfig{
                                      .types = {"cppwiki-auth-checker"},
                                  },
                          },
                  },
          },
  };
}

}  // namespace

auto RuntimeConfig::FromDefaults() -> RuntimeConfig {
  return RuntimeConfig{
      .config_path = std::string(cppwiki::constants::kDefaultServerConfigPath),
      .bind_host = std::nullopt,
      .port = std::nullopt,
      .log_level = std::nullopt,
      .swagger = false,
  };
}

auto RuntimeConfig::FromCli(int argc, char* argv[]) -> RuntimeConfig {
  RuntimeConfig cfg = FromDefaults();
  std::optional<std::string> cli_bind_host;
  std::optional<int> parsed_port;
  std::optional<std::string> cli_log_level;

  CLI::App app("CppWiki userver server");
  app.add_option("-c,--config", cfg.config_path, "Path to YAML server config file");
  app.add_option("--bind-host", cli_bind_host, "Bind host override");

  auto* port_option = app.add_option("--port", parsed_port, "Port override");
  port_option->check(CLI::Range(1, 65535));

  app.add_option("--log-level", cli_log_level,
                 "Log level override: trace, debug, info, warn, error, critical, off");

  bool swagger_enabled = false;
  bool swagger_disabled = false;
  app.add_flag("--swagger", swagger_enabled, "Enable Swagger UI (placeholder)");
  app.add_flag("--no-swagger", swagger_disabled, "Disable Swagger UI");

  app.parse(argc, argv);

  if (!cfg.config_path.empty() && cfg.config_path != "-") {
    const auto file_config = LoadRuntimeConfigFile(cfg.config_path);
    cfg.bind_host = file_config.bind_host;
    cfg.port = file_config.port;
    cfg.log_level = file_config.log_level;
  }

  if (cli_bind_host) {
    cfg.bind_host = std::move(cli_bind_host);
  }

  if (parsed_port) {
    cfg.port = static_cast<std::uint16_t>(*parsed_port);
  }

  if (cli_log_level) {
    cfg.log_level = ConvertLevel(*cli_log_level);
  }

  if (swagger_enabled && swagger_disabled) {
    throw std::runtime_error("Use only one of --swagger or --no-swagger.");
  }
  if (swagger_enabled) {
    cfg.swagger = true;
  } else if (swagger_disabled) {
    cfg.swagger = false;
  }

  return cfg;
}

auto RuntimeConfig::ToStaticConfigYaml() const -> std::string {
  const auto config = MakeStaticConfig(Host(), Port(), LogLevel());
  return rfl::yaml::write(config) + "\n";
}

auto RuntimeConfig::Host() const -> const std::string& {
  if (bind_host) {
    return *bind_host;
  }

  static const std::string kDefaultHost(cppwiki::constants::kDefaultServerBindHost);
  return kDefaultHost;
}

auto RuntimeConfig::Port() const -> std::uint16_t {
  if (port) {
    return *port;
  }

  return cppwiki::constants::kDefaultServerPort;
}

auto RuntimeConfig::LogLevel() const -> const std::string& {
  if (log_level) {
    return *log_level;
  }

  static const std::string kDefaultLevel(cppwiki::constants::kDefaultServerLogLevel);
  return kDefaultLevel;
}

}  // namespace cppwiki::server::config
