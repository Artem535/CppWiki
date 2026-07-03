#ifndef CPPWIKI_SRC_SERVER_COMPONENTS_SYNC_BOOTSTRAP_COMPONENT_H_
#define CPPWIKI_SRC_SERVER_COMPONENTS_SYNC_BOOTSTRAP_COMPONENT_H_

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <userver/clients/http/client.hpp>
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
  std::string admin_url;
};

class SyncBootstrapComponent final : public userver::components::ComponentBase {
 public:
  enum class AddWorkspaceResult {
    kAdded,
    kAlreadyExists,
    kPersistFailed,
  };

  static constexpr std::string_view kName = "sync-config";

  SyncBootstrapComponent(const userver::components::ComponentConfig& config,
                         const userver::components::ComponentContext& context);

  [[nodiscard]] static auto GetStaticConfigSchema() -> userver::yaml_config::Schema;
  [[nodiscard]] auto GetState() const -> const SyncBootstrapState&;
  [[nodiscard]] auto ListWorkspaces() const -> std::vector<std::string>;
  auto AddWorkspace(std::string workspace_id) -> AddWorkspaceResult;

 private:
  [[nodiscard]] static auto DeriveInitialWorkspaces(
      const SyncBootstrapState& state) -> std::set<std::string>;
  void LoadPersistedWorkspaces();
  [[nodiscard]] auto PersistWorkspacesLocked() -> bool;
  [[nodiscard]] auto RegistryDocumentUrl() const -> std::string;
  [[nodiscard]] auto KeyspaceDocumentUrl(std::string_view document_id) const -> std::string;
  auto PersistWorkspaceRootDocument(const std::string& workspace_id) -> bool;

  SyncBootstrapState state_;
  userver::clients::http::Client& http_client_;
  mutable std::mutex workspaces_mutex_;
  std::set<std::string> workspace_ids_;
};

}  // namespace cppwiki::server::components

#endif  // CPPWIKI_SRC_SERVER_COMPONENTS_SYNC_BOOTSTRAP_COMPONENT_H_
