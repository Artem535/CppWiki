#ifndef CPPWIKI_SRC_PLUGINS_API_UI_H_
#define CPPWIKI_SRC_PLUGINS_API_UI_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "plugins/api/plugin.h"
#include "plugins/surface.h"

class QWidget;

namespace cppwiki::plugins {

// What a native view receives from the host. Theme, layout, parenting and destruction stay
// with the host; the view only renders its own content and talks through these calls.
class IViewContext {
 public:
  virtual ~IViewContext() = default;

  [[nodiscard]] virtual auto PluginId() const -> std::string_view = 0;
  [[nodiscard]] virtual auto ContributionId() const -> std::string_view = 0;

  // Resolves a host theme token (color, spacing, font role) by name; empty when unknown.
  [[nodiscard]] virtual auto ThemeToken(std::string_view name) const -> std::string = 0;

  // Asks the host to run a platform command ("open-document", "run-search", ...). Returns
  // false when the command or its scope is not available to this plugin.
  virtual auto RunCommand(std::string_view command, std::string_view argument) -> bool = 0;

  // Publishes/updates a declarative surface owned by this contribution (a native view may
  // embed host-rendered content this way).
  virtual auto SetSurface(const SurfaceNode& surface) -> bool = 0;
};

// A Tier A plugin that contributes native views. One factory serves every native contribution
// the plugin declares; IViewContext identifies which contribution is being created.
class IViewPlugin : public IPlugin {
 public:
  virtual auto CreateView(IViewContext& context) -> QWidget* = 0;
};

// Contract for a document editor contributed to `document.editor`. The host owns the tab,
// saving, the conflict gate and revision history. Not wired by the v0 host yet; document kinds
// arrive with the meeting-record slice.
class IDocumentEditor {
 public:
  virtual ~IDocumentEditor() = default;
  virtual void Load(std::string_view snapshot_json) = 0;
  [[nodiscard]] virtual auto Snapshot() const -> std::string = 0;
  [[nodiscard]] virtual auto IsDirty() const -> bool = 0;
  [[nodiscard]] virtual auto Validate() const -> std::optional<std::string> = 0;
};

class IDocumentKindPlugin : public IViewPlugin {
 public:
  virtual auto CreateEditor(IViewContext& context) -> std::unique_ptr<IDocumentEditor> = 0;
};

}  // namespace cppwiki::plugins

#endif  // CPPWIKI_SRC_PLUGINS_API_UI_H_
