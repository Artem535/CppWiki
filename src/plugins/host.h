#ifndef CPPWIKI_SRC_PLUGINS_HOST_H_
#define CPPWIKI_SRC_PLUGINS_HOST_H_

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "plugins/api/plugin.h"
#include "plugins/api/services.h"
#include "plugins/display_registry.h"
#include "plugins/event_bus.h"
#include "plugins/executor.h"
#include "plugins/manifest.h"

namespace cppwiki::plugins {

// Platform-side providers injected into the host. Everything is nullable: a missing provider
// means deny, so tests can exercise the permission layer without a real repository/network.
struct PlatformServices {
  IDocumentAccess* documents = nullptr;
  IPluginSettings* settings = nullptr;
  std::filesystem::path plugin_data_root;
  IModelStore* models = nullptr;
  INetworkBroker* network = nullptr;
  ICredentials* credentials = nullptr;
  IPluginLog* log = nullptr;
  IAuditLog* audit = nullptr;
};

enum class PluginState { kRegistered, kLoaded, kStarted, kStopped, kFailed };

struct PluginSnapshot {
  std::string id;
  PluginState state = PluginState::kRegistered;
  std::string error;
};

// Tier A plugin host: validates manifests, resolves dependencies, runs the plugin lifecycle on
// the platform executor, enforces permissions and owns the event bus and display registry.
class PluginHost final {
 public:
  PluginHost(IExecutor& executor, PlatformServices services);
  ~PluginHost();

  PluginHost(const PluginHost&) = delete;
  PluginHost& operator=(const PluginHost&) = delete;

  // Registers a plugin bundle. Returns an empty string on success. The manifest is validated
  // immediately; the factory runs in LoadAll(), after dependency resolution.
  auto Register(std::string_view manifest_json, PluginTier tier, PluginFactory factory)
      -> std::string;

  // Instantiates plugins in dependency order and registers their display contributions.
  void LoadAll();

  // Starts every successfully loaded plugin on the executor. A failed start does not abort the
  // rest; plugins that depend on a failed plugin fail with a dependency error.
  void StartAll();

  // Stops started plugins in reverse order through the executor, waiting up to `deadline` for
  // the whole shutdown. A plugin that overruns is reported and left for process teardown.
  void StopAll(std::chrono::milliseconds deadline = std::chrono::seconds(5));

  [[nodiscard]] auto State(std::string_view id) const -> PluginState;
  [[nodiscard]] auto Error(std::string_view id) const -> std::string;
  [[nodiscard]] auto Snapshots() const -> std::vector<PluginSnapshot>;

  [[nodiscard]] auto Ui() -> UiRegistry&;
  [[nodiscard]] auto Bus() -> EventBus&;
  [[nodiscard]] auto AuditTrail() const -> const std::vector<AuditRecord>&;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace cppwiki::plugins

#endif  // CPPWIKI_SRC_PLUGINS_HOST_H_
