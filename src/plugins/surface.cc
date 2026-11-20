#include "plugins/surface.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace cppwiki::plugins {

namespace {

constexpr const char* kTypes[] = {"panel",     "section", "text",    "markdown", "list", "table",
                                  "key-value", "form",    "actions", "progress", "badge"};

auto IsKnownType(const std::string& type) -> bool {
  return std::any_of(std::begin(kTypes), std::end(kTypes),
                     [&type](const char* known) { return type == known; });
}

auto StringField(const QJsonObject& object, const char* key, std::string& out, std::string& error)
    -> bool {
  const auto value = object.value(QString::fromUtf8(key));
  if (value.isUndefined()) {
    return true;
  }
  if (!value.isString()) {
    error = std::string("surface field '") + key + "' must be a string";
    return false;
  }
  out = value.toString().toStdString();
  if (out.size() > kMaxSurfaceString) {
    error = std::string("surface field '") + key + "' exceeds " +
            std::to_string(kMaxSurfaceString) + " characters";
    return false;
  }
  return true;
}

auto ContainsUrl(std::string_view text) -> bool {
  return text.find("://") != std::string_view::npos;
}

auto CheckNoUrl(const std::string& text, std::string& error) -> bool {
  if (ContainsUrl(text)) {
    error = "surface strings must not contain URLs (" + text.substr(0, 80) + ")";
    return false;
  }
  return true;
}

auto ReadStringArray(const QJsonValue& value, const char* field, std::vector<std::string>& out,
                     std::string& error) -> bool {
  if (!value.isArray()) {
    error = std::string("surface field '") + field + "' must be an array of strings";
    return false;
  }
  const auto array = value.toArray();
  out.clear();
  out.reserve(static_cast<std::size_t>(array.size()));
  for (const auto& entry : array) {
    if (!entry.isString()) {
      error = std::string("surface field '") + field + "' must contain only strings";
      return false;
    }
    auto text = entry.toString().toStdString();
    if (text.size() > kMaxSurfaceString || !CheckNoUrl(text, error)) {
      return false;
    }
    out.push_back(std::move(text));
  }
  return true;
}

auto ParseNode(const QJsonObject& object, int depth, std::size_t& node_count, std::string& error)
    -> std::optional<SurfaceNode> {
  if (depth > kMaxSurfaceDepth) {
    error = "surface exceeds depth " + std::to_string(kMaxSurfaceDepth);
    return std::nullopt;
  }
  ++node_count;
  if (node_count > kMaxSurfaceNodes) {
    error = "surface exceeds " + std::to_string(kMaxSurfaceNodes) + " nodes";
    return std::nullopt;
  }

  SurfaceNode node;
  const auto type = object.value(QStringLiteral("type"));
  if (!type.isString() || type.toString().isEmpty()) {
    error = "surface node is missing a 'type'";
    return std::nullopt;
  }
  node.type = type.toString().toStdString();
  if (!IsKnownType(node.type)) {
    error = "unknown surface primitive '" + node.type + "'";
    return std::nullopt;
  }

  if (!StringField(object, "id", node.id, error) ||
      !StringField(object, "title", node.title, error) ||
      !StringField(object, "text", node.text, error) ||
      !StringField(object, "label", node.label, error) ||
      !StringField(object, "style", node.style, error)) {
    return std::nullopt;
  }
  for (const auto* value : {&node.id, &node.title, &node.text, &node.label, &node.style}) {
    if (!CheckNoUrl(*value, error)) {
      return std::nullopt;
    }
  }

  const auto children = object.value(QStringLiteral("children"));
  if (!children.isUndefined()) {
    if (!children.isArray()) {
      error = "surface field 'children' must be an array";
      return std::nullopt;
    }
    for (const auto& child : children.toArray()) {
      if (!child.isObject()) {
        error = "surface children must be objects";
        return std::nullopt;
      }
      auto parsed = ParseNode(child.toObject(), depth + 1, node_count, error);
      if (!parsed.has_value()) {
        return std::nullopt;
      }
      node.children.push_back(std::move(*parsed));
    }
  }

  if (node.type == "list") {
    if (!ReadStringArray(object.value(QStringLiteral("items")), "items", node.items, error)) {
      return std::nullopt;
    }
  } else if (node.type == "key-value") {
    const auto items = object.value(QStringLiteral("items"));
    if (!items.isArray()) {
      error = "key-value surface requires an 'items' array of [key, value] pairs";
      return std::nullopt;
    }
    for (const auto& entry : items.toArray()) {
      if (!entry.isArray()) {
        error = "key-value items must be [key, value] arrays";
        return std::nullopt;
      }
      const auto pair = entry.toArray();
      if (pair.size() != 2 || !pair.at(0).isString() || !pair.at(1).isString()) {
        error = "key-value items must be [key, value] string pairs";
        return std::nullopt;
      }
      auto key = pair.at(0).toString().toStdString();
      auto value = pair.at(1).toString().toStdString();
      if (!CheckNoUrl(key, error) || !CheckNoUrl(value, error)) {
        return std::nullopt;
      }
      node.key_values.emplace_back(std::move(key), std::move(value));
    }
  } else if (node.type == "table") {
    if (!ReadStringArray(object.value(QStringLiteral("columns")), "columns", node.columns, error)) {
      return std::nullopt;
    }
    const auto rows = object.value(QStringLiteral("rows"));
    if (!rows.isArray()) {
      error = "table surface requires a 'rows' array";
      return std::nullopt;
    }
    for (const auto& row : rows.toArray()) {
      if (!row.isArray()) {
        error = "table rows must be arrays";
        return std::nullopt;
      }
      std::vector<std::string> cells;
      if (!ReadStringArray(row, "rows", cells, error)) {
        return std::nullopt;
      }
      if (!node.columns.empty() && cells.size() != node.columns.size()) {
        error = "table row has " + std::to_string(cells.size()) + " cells but " +
                std::to_string(node.columns.size()) + " columns";
        return std::nullopt;
      }
      node.rows.push_back(std::move(cells));
    }
  } else if (node.type == "actions") {
    const auto actions = object.value(QStringLiteral("items"));
    if (!actions.isArray()) {
      error = "actions surface requires an 'items' array";
      return std::nullopt;
    }
    for (const auto& entry : actions.toArray()) {
      if (!entry.isObject()) {
        error = "action entries must be objects";
        return std::nullopt;
      }
      const auto action_object = entry.toObject();
      SurfaceAction action;
      const auto id = action_object.value(QStringLiteral("id"));
      const auto label = action_object.value(QStringLiteral("label"));
      if (!id.isString() || id.toString().isEmpty() || !label.isString() ||
          label.toString().isEmpty()) {
        error = "actions require non-empty 'id' and 'label' strings";
        return std::nullopt;
      }
      action.id = id.toString().toStdString();
      action.label = label.toString().toStdString();
      if (!CheckNoUrl(action.label, error)) {
        return std::nullopt;
      }
      const auto style = action_object.value(QStringLiteral("style"));
      if (!style.isUndefined()) {
        if (!style.isString()) {
          error = "action 'style' must be a string";
          return std::nullopt;
        }
        action.style = style.toString().toStdString();
      }
      node.actions.push_back(std::move(action));
    }
  } else if (node.type == "progress") {
    const auto value = object.value(QStringLiteral("value"));
    if (!value.isDouble()) {
      error = "progress surface requires a numeric 'value' between 0 and 1";
      return std::nullopt;
    }
    node.progress = value.toDouble();
    node.has_progress = true;
    if (node.progress < 0.0 || node.progress > 1.0) {
      error = "progress 'value' must be between 0 and 1";
      return std::nullopt;
    }
  }

  return node;
}

auto ValidateNode(const SurfaceNode& node, int depth, std::size_t& count, std::string& error)
    -> bool {
  if (depth > kMaxSurfaceDepth) {
    error = "surface exceeds depth " + std::to_string(kMaxSurfaceDepth);
    return false;
  }
  ++count;
  if (count > kMaxSurfaceNodes) {
    error = "surface exceeds " + std::to_string(kMaxSurfaceNodes) + " nodes";
    return false;
  }
  if (!IsKnownType(node.type)) {
    error = "unknown surface primitive '" + node.type + "'";
    return false;
  }
  for (const auto* value : {&node.id, &node.title, &node.text, &node.label, &node.style}) {
    if (value->size() > kMaxSurfaceString) {
      error = "surface string exceeds " + std::to_string(kMaxSurfaceString) + " characters";
      return false;
    }
    if (!CheckNoUrl(*value, error)) {
      return false;
    }
  }
  for (const auto& item : node.items) {
    if (!CheckNoUrl(item, error)) {
      return false;
    }
  }
  for (const auto& [key, value] : node.key_values) {
    if (!CheckNoUrl(key, error) || !CheckNoUrl(value, error)) {
      return false;
    }
  }
  for (const auto& cell : node.columns) {
    if (!CheckNoUrl(cell, error)) {
      return false;
    }
  }
  for (const auto& row : node.rows) {
    if (!node.columns.empty() && row.size() != node.columns.size()) {
      error = "table row width does not match columns";
      return false;
    }
    for (const auto& cell : row) {
      if (!CheckNoUrl(cell, error)) {
        return false;
      }
    }
  }
  for (const auto& action : node.actions) {
    if (action.id.empty() || action.label.empty()) {
      error = "actions require a non-empty id and label";
      return false;
    }
    if (!CheckNoUrl(action.label, error)) {
      return false;
    }
  }
  if (node.type == "progress" &&
      (!node.has_progress || node.progress < 0.0 || node.progress > 1.0)) {
    error = "progress 'value' must be between 0 and 1";
    return false;
  }
  for (const auto& child : node.children) {
    if (!ValidateNode(child, depth + 1, count, error)) {
      return false;
    }
  }
  return true;
}

}  // namespace

auto ParseSurface(std::string_view json) -> SurfaceParseResult {
  if (json.empty()) {
    return SurfaceParseResult{std::nullopt, "surface is empty"};
  }
  QJsonParseError parse_error{};
  const auto document =
      QJsonDocument::fromJson(QByteArray(json.data(), static_cast<int>(json.size())), &parse_error);
  if (parse_error.error != QJsonParseError::NoError) {
    return SurfaceParseResult{
        std::nullopt, "surface is not valid JSON: " + parse_error.errorString().toStdString()};
  }
  if (!document.isObject()) {
    return SurfaceParseResult{std::nullopt, "surface must be a JSON object"};
  }
  const auto object = document.object();
  const auto version = object.value(QStringLiteral("surface_version"));
  if (!version.isDouble() || version.toInt() != version.toDouble() ||
      version.toInt() != kSurfaceVersion) {
    return SurfaceParseResult{std::nullopt,
                              "surface_version must be " + std::to_string(kSurfaceVersion)};
  }
  std::size_t node_count = 0;
  std::string error;
  auto root = ParseNode(object, 0, node_count, error);
  if (!root.has_value()) {
    return SurfaceParseResult{std::nullopt, std::move(error)};
  }
  return SurfaceParseResult{std::move(root), {}};
}

auto ValidateSurface(const SurfaceNode& root) -> std::string {
  std::size_t node_count = 0;
  std::string error;
  if (!ValidateNode(root, 0, node_count, error)) {
    return error;
  }
  return {};
}

}  // namespace cppwiki::plugins
