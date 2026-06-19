#ifndef CPPWIKI_SRC_SERVER_CONFIG_RUNTIME_CONFIG_H_
#define CPPWIKI_SRC_SERVER_CONFIG_RUNTIME_CONFIG_H_

#include <cstdint>
#include <optional>
#include <string>

#include <CLI/CLI.hpp>

namespace cppwiki::server::config {

struct RuntimeConfig final {
  std::string config_path;
  std::optional<std::string> bind_host;
  std::optional<std::uint16_t> port;
  std::optional<std::string> log_level;
  bool swagger{false};

  [[nodiscard]] static auto FromDefaults() -> RuntimeConfig;
  [[nodiscard]] static auto FromCli(int argc, char* argv[]) -> RuntimeConfig;
  [[nodiscard]] auto ToStaticConfigYaml() const -> std::string;

  [[nodiscard]] auto Host() const -> const std::string&;
  [[nodiscard]] auto Port() const -> std::uint16_t;
  [[nodiscard]] auto LogLevel() const -> const std::string&;
};

}  // namespace cppwiki::server::config

#endif  // CPPWIKI_SRC_SERVER_CONFIG_RUNTIME_CONFIG_H_
