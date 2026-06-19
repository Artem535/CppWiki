#include "server/config/runtime_config.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <rfl/yaml/read.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "core/constants.h"

namespace cppwiki::server::config {

namespace {

struct RuntimeConfigFile final {
  std::optional<std::string> bind_host;
  std::optional<std::uint16_t> port;
  std::optional<std::string> log_level;
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
  std::string host = Host();
  if (host == "0.0.0.0") {
    host = "0.0.0.0";
  }

  std::ostringstream out;
  out << "# Auto-generated userver static config for CppWiki server\n";
  out << "components_manager:\n";
  out << "  coro_pool:\n";
  out << "    initial_size: 4\n";
  out << "    max_size: 128\n";
  out << "  task_processors:\n";
  out << "    main-task-processor:\n";
  out << "      worker_threads: 4\n";
  out << "      thread_name: main-worker\n";
  out << "  default_task_processor: main-task-processor\n";
  out << "  components:\n";
  out << "    logging:\n";
  out << "      fs_task_processor: main-task-processor\n";
  out << "      loggers:\n";
  out << "        default:\n";
  out << "          file_path: '@null'\n";
  out << "          level: " << LogLevel() << "\n";
  out << "          overflow_behavior: discard\n";
  out << "        default\n";
  out << "    server:\n";
  out << "      listener:\n";
  out << "        port: " << Port() << "\n";
  out << "        address: '" << host << "'\n";
  out << "        task_processor: main-task-processor\n";
  out << "        connection:\n";
  out << "          in_buffer_size: 32768\n";
  out << "          out_buffer_size: 32768\n";
  out << "    handler-health:\n";
  out << "      path: /api/v1/health\n";
  out << "      method: GET\n";
  out << "      task_processor: main-task-processor\n";
  out << "    handler-options:\n";
  out << "      path: /api/v1/health\n";
  out << "      method: OPTIONS\n";
  out << "      task_processor: main-task-processor\n";
  out << "    handler-locks:\n";
  out << "      path: /api/v1/locks/{document_id}\n";
  out << "      method: '*'\n";
  out << "      task_processor: main-task-processor\n";
  out << "    handler-presence:\n";
  out << "      path: /api/v1/presence/{workspace_id}\n";
  out << "      method: '*'\n";
  out << "      task_processor: main-task-processor\n";
  out << "    auth-checker:\n";
  out << "      path: /api/v1/locks/{document_id}\n";
  out << "      method: '*'\n";
  out << "    auth-checker-presence:\n";
  out << "      path: /api/v1/presence/{workspace_id}\n";
  out << "      method: '*'\n";
  out << "...\n";

  return out.str();
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
