#ifndef CPPWIKI_SRC_PLUGINS_DISPLAY_REGISTRY_H_
#define CPPWIKI_SRC_PLUGINS_DISPLAY_REGISTRY_H_

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "plugins/api/services.h"
#include "plugins/api/ui.h"
#include "plugins/manifest.h"
#include "plugins/surface.h"

class QWidget;

namespace cppwiki::plugins {

struct SlotDefinition {
  std::string_view id;
  bool allow_native = false;
  bool allow_declarative = false;
  bool multiple = true;
  bool requires_document_kind = false;
};

[[nodiscard]] auto PluginSlots() -> const std::vector<SlotDefinition>&;
[[nodiscard]] auto FindPluginSlot(std::string_view id) -> const SlotDefinition*;

struct ContributionRecord {
  PluginContribution contribution;
  std::string plugin_id;
  PluginTier tier = PluginTier::kNative;
  bool active = false;
  QWidget* view = nullptr;
};

// Host-owned display registry: validates manifest contributions, creates native views lazily
// and stores declarative surfaces until a renderer consumes them.
class UiRegistry final {
 public:
  UiRegistry() = default;
  UiRegistry(const UiRegistry&) = delete;
  UiRegistry& operator=(const UiRegistry&) = delete;

  // Returns an empty string on success, otherwise the reason the contribution set is invalid.
  // Validate() checks the same rules against the registry's current state without mutating it;
  // Register() validates and then adds the contributions.
  [[nodiscard]] auto Validate(const PluginManifest& manifest, PluginTier tier,
                              bool has_native_factory) const -> std::string;
  auto Register(const PluginManifest& manifest, PluginTier tier,
                std::function<QWidget*(IViewContext&)> native_factory) -> std::string;

  [[nodiscard]] auto Contributions(std::string_view slot) const
      -> std::vector<const ContributionRecord*>;
  [[nodiscard]] auto AllContributions() const -> const std::vector<ContributionRecord>&;
  [[nodiscard]] auto Find(std::string_view plugin_id, std::string_view contribution_id) const
      -> const ContributionRecord*;

  // Creates the native view on first activation. Returns false for unknown contributions,
  // declarative-only contributions, or a factory that returns null.
  auto Activate(std::string_view plugin_id, std::string_view contribution_id, IViewContext& context)
      -> bool;
  void Deactivate(std::string_view plugin_id, std::string_view contribution_id);
  [[nodiscard]] auto IsActive(std::string_view plugin_id, std::string_view contribution_id) const
      -> bool;

  auto UpdateSurface(std::string_view plugin_id, std::string_view contribution_id,
                     const SurfaceNode& surface) -> bool;
  [[nodiscard]] auto Surface(std::string_view plugin_id, std::string_view contribution_id) const
      -> const SurfaceNode*;

  auto SetSurfaceEventSink(std::string_view plugin_id, std::string_view contribution_id,
                           std::function<void(const SurfaceEvent&)> sink) -> bool;
  // Delivers an interaction to the plugin that owns `plugin_id`/`event.surface_id`.
  auto Dispatch(const std::string& plugin_id, const SurfaceEvent& event) -> bool;

  // Renderer hook: called whenever a plugin publishes a new surface tree.
  void SetSurfaceObserver(
      std::function<void(const std::string& plugin_id, const std::string& contribution_id,
                         const SurfaceNode&)>
          observer);

  // Drops every contribution, surface and sink owned by a stopped/failed plugin.
  void RemovePlugin(std::string_view plugin_id);

  void Clear();

 private:
  using Key = std::pair<std::string, std::string>;

  [[nodiscard]] auto FindMutable(std::string_view plugin_id, std::string_view contribution_id)
      -> ContributionRecord*;

  std::vector<ContributionRecord> records_;
  std::map<Key, std::function<QWidget*(IViewContext&)>> factories_;
  std::map<Key, SurfaceNode> surfaces_;
  std::map<Key, std::function<void(const SurfaceEvent&)>> event_sinks_;
  std::function<void(const std::string&, const std::string&, const SurfaceNode&)> surface_observer_;
};

}  // namespace cppwiki::plugins

#endif  // CPPWIKI_SRC_PLUGINS_DISPLAY_REGISTRY_H_
