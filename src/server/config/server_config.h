#ifndef CPPWIKI_SRC_SERVER_SERVER_CONFIG_H_
#define CPPWIKI_SRC_SERVER_SERVER_CONFIG_H_

#include <cstdint>
#include <optional>
#include <string>

namespace cppwiki::server {

class ServerConfig final {
 public:
  ServerConfig(std::string bind_host, std::uint16_t port, bool enable_swagger,
               std::string log_level);

  [[nodiscard]] static auto FromDefaults() -> ServerConfig;
  [[nodiscard]] static auto FromYamlFile(const std::string& path) -> ServerConfig;
  [[nodiscard]] auto WithOverrides(const std::optional<std::string>& bind_host_override,
                                   const std::optional<std::uint16_t>& port_override,
                                   const std::optional<bool>& swagger_override,
                                   const std::optional<std::string>& log_level_override) const
      -> ServerConfig;

  [[nodiscard]] auto BindHost() const -> const std::string&;
  [[nodiscard]] auto Port() const -> std::uint16_t;
  [[nodiscard]] auto SwaggerEnabled() const -> bool;
  [[nodiscard]] auto LogLevel() const -> const std::string&;

 private:
  std::string bind_host_;
  std::uint16_t port_;
  bool enable_swagger_;
  std::string log_level_;
};

}  // namespace cppwiki::server

#endif  // CPPWIKI_SRC_SERVER_SERVER_CONFIG_H_
