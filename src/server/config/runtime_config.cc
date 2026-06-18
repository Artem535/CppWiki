#include "server/config/runtime_config.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "core/constants.h"

namespace cppwiki::server::config {

namespace {

auto Normalize(std::string_view value) -> std::string {
  std::string out(value);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
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
  const auto normalized = Normalize(level);
  if (normalized == "trace" || normalized == "debug" || normalized == "info" ||
      normalized == "warn" || normalized == "error" || normalized == "critical" ||
      normalized == "off" || normalized == "warning") {
    return normalized;
  }
  throw std::invalid_argument("Unsupported log level: " + level);
}

auto Trim(std::string_view value) -> std::string_view {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
    value.remove_prefix(1);
  }
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
    value.remove_suffix(1);
  }
  return value;
}

auto Unquote(std::string_view value) -> std::string {
  value = Trim(value);
  if (value.size() >= 2 &&
      ((value.front() == '\'' && value.back() == '\'') ||
       (value.front() == '"' && value.back() == '"'))) {
    value.remove_prefix(1);
    value.remove_suffix(1);
  }
  return std::string(value);
}

auto FindYamlScalar(std::string_view yaml, std::string_view key) -> std::optional<std::string> {
  std::istringstream input{std::string(yaml)};
  std::string line;
  const std::string prefix = std::string(key) + ":";
  while (std::getline(input, line)) {
    const auto comment_pos = line.find('#');
    if (comment_pos != std::string::npos) {
      line.erase(comment_pos);
    }

    const auto trimmed = Trim(line);
    if (!trimmed.starts_with(prefix)) {
      continue;
    }

    return Unquote(trimmed.substr(prefix.size()));
  }

  return std::nullopt;
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

  CLI::App app("CppWiki userver server");
  app.add_option("-c,--config", cfg.config_path, "Path to YAML server config file");
  app.add_option("--bind-host", cfg.bind_host, "Bind host override");

  std::optional<int> parsed_port;
  auto* port_option = app.add_option("--port", parsed_port, "Port override");
  port_option->check(CLI::Range(1, 65535));

  app.add_option("--log-level", cfg.log_level,
                 "Log level override: trace, debug, info, warn, error, critical, off");

  bool swagger_enabled = false;
  bool swagger_disabled = false;
  app.add_flag("--swagger", swagger_enabled, "Enable Swagger UI (placeholder)");
  app.add_flag("--no-swagger", swagger_disabled, "Disable Swagger UI");

  app.parse(argc, argv);

  if (parsed_port) {
    cfg.port = static_cast<std::uint16_t>(*parsed_port);
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

  if (!config_path.empty() && config_path != "-") {
    try {
      const auto yaml = ReadFile(config_path);
      if (const auto parsed = FindYamlScalar(yaml, "bind_host")) {
        const_cast<RuntimeConfig*>(this)->bind_host = *parsed;
        return *bind_host;
      }
    } catch (const std::exception&) {
      // Fall through to defaults.
    }
  }

  static const std::string kDefaultHost(cppwiki::constants::kDefaultServerBindHost);
  return kDefaultHost;
}

auto RuntimeConfig::Port() const -> std::uint16_t {
  if (port) {
    return *port;
  }

  if (!config_path.empty() && config_path != "-") {
    try {
      const auto yaml = ReadFile(config_path);
      if (const auto parsed = FindYamlScalar(yaml, "port")) {
        const_cast<RuntimeConfig*>(this)->port = ConvertToUint16(*parsed);
        return *port;
      }
    } catch (const std::exception&) {
      // Fall through to defaults.
    }
  }

  return cppwiki::constants::kDefaultServerPort;
}

auto RuntimeConfig::LogLevel() const -> const std::string& {
  if (log_level) {
    return *log_level;
  }

  if (!config_path.empty() && config_path != "-") {
    try {
      const auto yaml = ReadFile(config_path);
      if (const auto parsed = FindYamlScalar(yaml, "log_level")) {
        const_cast<RuntimeConfig*>(this)->log_level = ConvertLevel(*parsed);
        return *log_level;
      }
    } catch (const std::exception&) {
      // Fall through to defaults.
    }
  }

  static const std::string kDefaultLevel(cppwiki::constants::kDefaultServerLogLevel);
  return kDefaultLevel;
}

}  // namespace cppwiki::server::config
