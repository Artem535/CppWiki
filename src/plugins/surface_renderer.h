#ifndef CPPWIKI_SRC_PLUGINS_SURFACE_RENDERER_H_
#define CPPWIKI_SRC_PLUGINS_SURFACE_RENDERER_H_

#include <QWidget>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "plugins/surface.h"

namespace cppwiki::plugins {

struct SurfaceRenderContext {
  std::string surface_id;
  std::function<void(const SurfaceEvent&)> on_event;
};

// Renders a validated declarative surface into Qt widgets. The host owns the returned widget:
// it sets the parent, shows/hides it and destroys it. Action clicks return the current form
// values as a SurfaceEvent; the renderer never calls plugin code directly.
class QtSurfaceRenderer final {
 public:
  QtSurfaceRenderer() = default;

  auto Render(const SurfaceNode& root, const SurfaceRenderContext& context,
              QWidget* parent = nullptr) -> QWidget*;

 private:
  auto Build(const SurfaceNode& node, QWidget* parent) -> QWidget*;
  auto BuildForm(const SurfaceNode& node, QWidget* parent) -> QWidget*;
  auto BuildActions(const SurfaceNode& node, QWidget* parent) -> QWidget*;
  [[nodiscard]] auto CollectValues() const -> std::map<std::string, std::string>;
  void ClearFields();

  SurfaceRenderContext context_;
  std::vector<std::pair<std::string, std::function<std::string()>>> fields_;
};

}  // namespace cppwiki::plugins

#endif  // CPPWIKI_SRC_PLUGINS_SURFACE_RENDERER_H_
