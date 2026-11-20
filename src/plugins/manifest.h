#ifndef CPPWIKI_SRC_PLUGINS_MANIFEST_H_
#define CPPWIKI_SRC_PLUGINS_MANIFEST_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cppwiki::plugins {

// Plugin API major version this platform implements (design: "api_version": 1).
inline constexpr int kPluginApiVersion = 1;

enum class PluginKind { kDocKind, kView, kService, kConnector, kSearchProvider };

enum class PluginTier { kNative, kWasm };

enum class SurfaceKind { kNative, kDeclarative };

[[nodiscard]] auto ToString(PluginKind kind) -> std::string_view;
[[nodiscard]] auto ToString(PluginTier tier) -> std::string_view;
[[nodiscard]] auto ToString(SurfaceKind kind) -> std::string_view;

[[nodiscard]] auto PluginKindFromString(std::string_view value) -> std::optional<PluginKind>;
[[nodiscard]] auto SurfaceKindFromString(std::string_view value) -> std::optional<SurfaceKind>;

// Declared capabilities, deny by default. A granted flag with an empty kind list means "all
// kinds"; `"*"` is accepted as an explicit spelling of the same thing.
struct PermissionSet {
  bool documents_read = false;
  bool documents_write = false;
  std::vector<std::string> document_read_kinds;
  std::vector<std::string> document_write_kinds;

  bool attachments_read = false;
  bool attachments_write = false;

  bool filesystem_plugin_data = false;
  bool models_read = false;

  std::vector<std::string> network_hosts;
  std::vector<std::string> credential_ids;
  std::vector<std::string> events_publish;
  std::vector<std::string> events_subscribe;

  [[nodiscard]] auto CanReadKind(std::string_view kind) const -> bool;
  [[nodiscard]] auto CanWriteKind(std::string_view kind) const -> bool;
  [[nodiscard]] auto CanPublish(std::string_view topic) const -> bool;
  [[nodiscard]] auto CanSubscribe(std::string_view topic) const -> bool;
  [[nodiscard]] auto CanReachHost(std::string_view host) const -> bool;
  [[nodiscard]] auto CanUseCredential(std::string_view id) const -> bool;
};

struct PluginContribution {
  std::string id;
  std::string slot;
  std::string title;
  SurfaceKind surface = SurfaceKind::kNative;
  // Only meaningful for the `document.editor` slot.
  std::string document_kind;
};

struct PluginManifest {
  std::string id;
  std::string name;
  std::string version;
  int api_version = 0;
  std::string min_platform_version;
  PluginKind kind = PluginKind::kService;
  std::vector<std::string> dependencies;
  std::vector<PluginContribution> contributes;
  PermissionSet permissions;
};

struct ManifestResult {
  std::optional<PluginManifest> manifest;
  std::string error;
};

// Parses and validates a plugin manifest against the v0 schema. Slot names and tier-specific
// contribution rules are validated by the host, which knows the slots and the plugin tier.
[[nodiscard]] auto ParseManifest(std::string_view json) -> ManifestResult;

// True for `<domain>.<subject>.v<major>` topics: lowercase letters, digits, dots and dashes,
// with a trailing `.vN` major suffix.
[[nodiscard]] auto ValidateTopic(std::string_view topic) -> bool;

}  // namespace cppwiki::plugins

#endif  // CPPWIKI_SRC_PLUGINS_MANIFEST_H_
