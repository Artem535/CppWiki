#ifndef CPPWIKI_SRC_SERVER_COMPONENTS_SYNC_BOOTSTRAP_COMPONENT_H_
#define CPPWIKI_SRC_SERVER_COMPONENTS_SYNC_BOOTSTRAP_COMPONENT_H_

#include <map>
#include <string>
#include <vector>

#include <userver/components/component_base.hpp>
#include <userver/yaml_config/schema.hpp>

namespace cppwiki::server::components {

struct SyncBootstrapState final {
  bool available{true};
  bool enabled{false};
  std::string gateway_url;
  std::string database_name;
  std::map<std::string, std::vector<std::string>> role_channels;
  std::map<std::string, std::vector<std::string>> group_channels;
  std::string status_text;
};

class SyncBootstrapComponent final : public userver::components::ComponentBase {
 public:
  static constexpr std::string_view kName = "sync-config";

  SyncBootstrapComponent(const userver::components::ComponentConfig& config,
                         const userver::components::ComponentContext& context);

  [[nodiscard]] static auto GetStaticConfigSchema() -> userver::yaml_config::Schema;
  [[nodiscard]] auto GetState() const -> const SyncBootstrapState&;

 private:
  SyncBootstrapState state_;
};

}  // namespace cppwiki::server::components

#endif  // CPPWIKI_SRC_SERVER_COMPONENTS_SYNC_BOOTSTRAP_COMPONENT_H_
