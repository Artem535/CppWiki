#include "server/config/server_config.h"

#include <rfl/yaml/load.hpp>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "core/constants.h"
#include "server/config/server_config_dto.h"

namespace cppwiki::server {

namespace {

[[nodiscard]] auto LoadConfigDto(const std::string& path) -> ServerConfigDto {
  const auto parsed = rfl::yaml::load<ServerConfigDto>(path);
  if (!parsed) {
    throw std::runtime_error("Failed to parse server config file '" + path + "': " +
                             parsed.error().what());
  }

  return std::move(parsed).value();
}

[[nodiscard]] auto ApplyDto(const ServerConfigDto& dto) -> ServerConfig {
  return ServerConfig::FromDefaults().WithOverrides(dto.bind_host, dto.port,
                                                    dto.enable_swagger, dto.log_level);
}

}  // namespace

ServerConfig::ServerConfig(std::string bind_host, std::uint16_t port, bool enable_swagger,
                           std::string log_level)
    : bind_host_(std::move(bind_host)),
      port_(port),
      enable_swagger_(enable_swagger),
      log_level_(std::move(log_level)) {}

auto ServerConfig::FromDefaults() -> ServerConfig {
  return ServerConfig(std::string(constants::kDefaultServerBindHost),
                      constants::kDefaultServerPort,
                      constants::kDefaultServerSwaggerEnabled,
                      std::string(constants::kDefaultServerLogLevel));
}

auto ServerConfig::FromYamlFile(const std::string& path) -> ServerConfig {
  return ApplyDto(LoadConfigDto(path));
}

auto ServerConfig::WithOverrides(const std::optional<std::string>& bind_host_override,
                                 const std::optional<std::uint16_t>& port_override,
                                 const std::optional<bool>& swagger_override,
                                 const std::optional<std::string>& log_level_override) const
    -> ServerConfig {
  return ServerConfig(bind_host_override.value_or(bind_host_),
                      port_override.value_or(port_),
                      swagger_override.value_or(enable_swagger_),
                      log_level_override.value_or(log_level_));
}

auto ServerConfig::BindHost() const -> const std::string& {
  return bind_host_;
}

auto ServerConfig::Port() const -> std::uint16_t {
  return port_;
}

auto ServerConfig::SwaggerEnabled() const -> bool {
  return enable_swagger_;
}

auto ServerConfig::LogLevel() const -> const std::string& {
  return log_level_;
}

}  // namespace cppwiki::server
