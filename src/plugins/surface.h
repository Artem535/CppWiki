#ifndef CPPWIKI_SRC_PLUGINS_SURFACE_H_
#define CPPWIKI_SRC_PLUGINS_SURFACE_H_

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppwiki::plugins {

// Declarative display primitives (Plugin API v0 design, "Declarative surfaces"). A plugin
// builds this tree and the host renders it; no HTML, CSS or arbitrary URLs cross the boundary.
inline constexpr int kSurfaceVersion = 1;
inline constexpr std::size_t kMaxSurfaceNodes = 1000;
inline constexpr int kMaxSurfaceDepth = 8;
inline constexpr std::size_t kMaxSurfaceString = 4096;

struct SurfaceAction {
  std::string id;
  std::string label;
  std::string style;
};

struct SurfaceNode {
  std::string type;
  std::string id;
  std::string title;
  std::string text;
  std::string label;
  std::string style;
  std::vector<SurfaceNode> children;
  // `list`: plain items; `key-value`: two-element [key, value] entries.
  std::vector<std::string> items;
  std::vector<std::pair<std::string, std::string>> key_values;
  std::vector<std::string> columns;
  std::vector<std::vector<std::string>> rows;
  std::vector<SurfaceAction> actions;
  double progress = 0.0;
  bool has_progress = false;
};

struct SurfaceEvent {
  std::string surface_id;
  std::string action_id;
  std::map<std::string, std::string> values;
};

struct SurfaceParseResult {
  std::optional<SurfaceNode> root;
  std::string error;
};

// Parses a `surface_version: 1` JSON tree and enforces the size, depth, primitive and string
// rules. Also usable for surfaces a plugin builds in code before publishing them.
[[nodiscard]] auto ParseSurface(std::string_view json) -> SurfaceParseResult;

// Validates an already-built tree (same rules as ParseSurface, without JSON).
[[nodiscard]] auto ValidateSurface(const SurfaceNode& root) -> std::string;

}  // namespace cppwiki::plugins

#endif  // CPPWIKI_SRC_PLUGINS_SURFACE_H_
