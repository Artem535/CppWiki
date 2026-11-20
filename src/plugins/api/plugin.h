#ifndef CPPWIKI_SRC_PLUGINS_API_PLUGIN_H_
#define CPPWIKI_SRC_PLUGINS_API_PLUGIN_H_

#include <functional>
#include <memory>

namespace cppwiki::plugins {

class IPluginContext;

// Minimal Tier A plugin contract. Kind-specific interfaces (search provider, connector, ...)
// are added with the vertical slices; every plugin still starts and stops through this.
class IPlugin {
 public:
  virtual ~IPlugin() = default;

  // Runs on the platform executor, never on the UI thread. Returning false marks the plugin
  // failed for the session; the host keeps running the other plugins.
  virtual auto Start(IPluginContext& context) -> bool {
    (void)context;
    return true;
  }

  // Called off the UI thread once the plugin was started. Must respect the host deadline.
  virtual void Stop() {}
};

using PluginFactory = std::function<std::unique_ptr<IPlugin>()>;

}  // namespace cppwiki::plugins

#endif  // CPPWIKI_SRC_PLUGINS_API_PLUGIN_H_
