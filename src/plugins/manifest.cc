#include "plugins/manifest.h"

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

auto Fail(std::string message) -> ManifestResult {
  return ManifestResult{std::nullopt, std::move(message)};
}

auto Contains(const std::vector<std::string>& values, std::string_view candidate) -> bool {
  return std::any_of(values.begin(), values.end(),
                     [candidate](const std::string& value) { return value == candidate; });
}

auto HasAllKinds(const std::vector<std::string>& kinds, std::string_view kind) -> bool {
  return kinds.empty() || Contains(kinds, "*") || Contains(kinds, kind);
}

auto IsValidId(std::string_view id) -> bool {
  if (id.empty()) {
    return false;
  }
  const auto is_lower = [](char ch) { return ch >= 'a' && ch <= 'z'; };
  const auto is_digit = [](char ch) { return ch >= '0' && ch <= '9'; };
  if (!is_lower(id.front()) && !is_digit(id.front())) {
    return false;
  }
  if (id.find('.') == std::string_view::npos) {
    return false;
  }
  for (const char ch : id) {
    if (!is_lower(ch) && !is_digit(ch) && ch != '.' && ch != '-') {
      return false;
    }
  }
  return id.back() != '.' && id.find("..") == std::string_view::npos;
}

auto ReadRequiredString(const QJsonObject& object, std::string_view key, std::string& out,
                        std::string& error) -> bool {
  const auto value = object.value(QString::fromUtf8(key.data(), static_cast<int>(key.size())));
  if (!value.isString() || value.toString().isEmpty()) {
    error = "manifest field '" + std::string(key) + "' must be a non-empty string";
    return false;
  }
  out = value.toString().toStdString();
  return true;
}

auto ReadStringArray(const QJsonValue& value, std::string_view field, std::vector<std::string>& out,
                     std::string& error) -> bool {
  if (!value.isArray()) {
    error = "manifest field '" + std::string(field) + "' must be an array of strings";
    return false;
  }
  const auto array = value.toArray();
  out.clear();
  out.reserve(static_cast<std::size_t>(array.size()));
  for (const auto& entry : array) {
    if (!entry.isString() || entry.toString().isEmpty()) {
      error = "manifest field '" + std::string(field) + "' must contain only non-empty strings";
      return false;
    }
    out.push_back(entry.toString().toStdString());
  }
  return true;
}

auto ReadStringOrArray(const QJsonValue& value, std::string_view field,
                       std::vector<std::string>& out, std::string& error) -> bool {
  if (value.isString()) {
    out.clear();
    out.push_back(value.toString().toStdString());
    return true;
  }
  return ReadStringArray(value, field, out, error);
}

auto ParsePermissions(const QJsonValue& value, PermissionSet& permissions, std::string& error)
    -> bool {
  if (value.isUndefined()) {
    return true;
  }
  if (!value.isObject()) {
    error = "manifest field 'permissions' must be an object";
    return false;
  }
  const auto object = value.toObject();
  for (auto it = object.begin(); it != object.end(); ++it) {
    const auto key = it.key().toStdString();
    const auto entry = it.value();

    if (key == "documents") {
      if (!entry.isObject()) {
        error = "permissions.documents must be an object";
        return false;
      }
      const auto documents = entry.toObject();
      for (auto doc_it = documents.begin(); doc_it != documents.end(); ++doc_it) {
        const auto scope = doc_it.key().toStdString();
        if (scope == "read") {
          permissions.documents_read = true;
          if (!ReadStringOrArray(doc_it.value(), "permissions.documents.read",
                                 permissions.document_read_kinds, error)) {
            return false;
          }
        } else if (scope == "write") {
          permissions.documents_write = true;
          if (!ReadStringOrArray(doc_it.value(), "permissions.documents.write",
                                 permissions.document_write_kinds, error)) {
            return false;
          }
        } else {
          error = "unknown documents permission '" + scope + "'";
          return false;
        }
      }
    } else if (key == "attachments") {
      if (!entry.isObject()) {
        error = "permissions.attachments must be an object";
        return false;
      }
      const auto attachments = entry.toObject();
      for (auto att_it = attachments.begin(); att_it != attachments.end(); ++att_it) {
        const auto scope = att_it.key().toStdString();
        if (!att_it.value().isBool()) {
          error = "permissions.attachments." + scope + " must be a boolean";
          return false;
        }
        if (scope == "read") {
          permissions.attachments_read = att_it.value().toBool();
        } else if (scope == "write") {
          permissions.attachments_write = att_it.value().toBool();
        } else {
          error = "unknown attachments permission '" + scope + "'";
          return false;
        }
      }
    } else if (key == "filesystem") {
      std::vector<std::string> scopes;
      if (!ReadStringArray(entry, "permissions.filesystem", scopes, error)) {
        return false;
      }
      for (const auto& scope : scopes) {
        if (scope != "plugin-data") {
          error = "unknown filesystem permission '" + scope + "'";
          return false;
        }
      }
      permissions.filesystem_plugin_data = !scopes.empty();
    } else if (key == "models") {
      std::vector<std::string> scopes;
      if (!ReadStringArray(entry, "permissions.models", scopes, error)) {
        return false;
      }
      for (const auto& scope : scopes) {
        if (scope != "read") {
          error = "unknown models permission '" + scope + "'";
          return false;
        }
      }
      permissions.models_read = !scopes.empty();
    } else if (key == "network") {
      if (!ReadStringArray(entry, "permissions.network", permissions.network_hosts, error)) {
        return false;
      }
    } else if (key == "credentials") {
      if (!ReadStringArray(entry, "permissions.credentials", permissions.credential_ids, error)) {
        return false;
      }
    } else if (key == "events") {
      if (!entry.isObject()) {
        error = "permissions.events must be an object";
        return false;
      }
      const auto events = entry.toObject();
      for (auto event_it = events.begin(); event_it != events.end(); ++event_it) {
        const auto scope = event_it.key().toStdString();
        if (scope == "publish") {
          if (!ReadStringArray(event_it.value(), "permissions.events.publish",
                               permissions.events_publish, error)) {
            return false;
          }
        } else if (scope == "subscribe") {
          if (!ReadStringArray(event_it.value(), "permissions.events.subscribe",
                               permissions.events_subscribe, error)) {
            return false;
          }
        } else {
          error = "unknown events permission '" + scope + "'";
          return false;
        }
      }
    } else {
      error = "unknown permission '" + key + "'";
      return false;
    }
  }
  return true;
}

auto ParseContributions(const QJsonValue& value, std::vector<PluginContribution>& out,
                        std::string& error) -> bool {
  if (value.isUndefined()) {
    return true;
  }
  if (!value.isArray()) {
    error = "manifest field 'contributes' must be an array";
    return false;
  }
  const auto array = value.toArray();
  out.clear();
  out.reserve(static_cast<std::size_t>(array.size()));
  for (const auto& entry : array) {
    if (!entry.isObject()) {
      error = "each manifest contribution must be an object";
      return false;
    }
    const auto object = entry.toObject();
    PluginContribution contribution;
    if (!ReadRequiredString(object, "id", contribution.id, error)) {
      return false;
    }
    if (!ReadRequiredString(object, "slot", contribution.slot, error)) {
      return false;
    }
    const auto title = object.value(QStringLiteral("title"));
    if (!title.isUndefined()) {
      if (!title.isString() || title.toString().isEmpty()) {
        error = "contribution 'title' must be a non-empty string";
        return false;
      }
      contribution.title = title.toString().toStdString();
    }
    const auto surface = object.value(QStringLiteral("surface"));
    if (!surface.isUndefined()) {
      if (!surface.isString()) {
        error = "contribution 'surface' must be a string";
        return false;
      }
      const auto parsed = SurfaceKindFromString(surface.toString().toStdString());
      if (!parsed.has_value()) {
        error = "unknown surface kind '" + surface.toString().toStdString() + "'";
        return false;
      }
      contribution.surface = *parsed;
    }
    const auto document_kind = object.value(QStringLiteral("document_kind"));
    if (!document_kind.isUndefined()) {
      if (!document_kind.isString() || document_kind.toString().isEmpty()) {
        error = "contribution 'document_kind' must be a non-empty string";
        return false;
      }
      contribution.document_kind = document_kind.toString().toStdString();
    }
    if (std::any_of(out.begin(), out.end(), [&contribution](const PluginContribution& existing) {
          return existing.id == contribution.id;
        })) {
      error = "duplicate contribution id '" + contribution.id + "'";
      return false;
    }
    out.push_back(std::move(contribution));
  }
  return true;
}

}  // namespace

auto ToString(PluginKind kind) -> std::string_view {
  switch (kind) {
    case PluginKind::kDocKind:
      return "doc-kind";
    case PluginKind::kView:
      return "view";
    case PluginKind::kService:
      return "service";
    case PluginKind::kConnector:
      return "connector";
    case PluginKind::kSearchProvider:
      return "search-provider";
  }
  return "service";
}

auto ToString(PluginTier tier) -> std::string_view {
  return tier == PluginTier::kNative ? "native" : "wasm";
}

auto ToString(SurfaceKind kind) -> std::string_view {
  return kind == SurfaceKind::kNative ? "native" : "declarative";
}

auto PluginKindFromString(std::string_view value) -> std::optional<PluginKind> {
  if (value == "doc-kind") {
    return PluginKind::kDocKind;
  }
  if (value == "view") {
    return PluginKind::kView;
  }
  if (value == "service") {
    return PluginKind::kService;
  }
  if (value == "connector") {
    return PluginKind::kConnector;
  }
  if (value == "search-provider") {
    return PluginKind::kSearchProvider;
  }
  return std::nullopt;
}

auto SurfaceKindFromString(std::string_view value) -> std::optional<SurfaceKind> {
  if (value == "native") {
    return SurfaceKind::kNative;
  }
  if (value == "declarative") {
    return SurfaceKind::kDeclarative;
  }
  return std::nullopt;
}

auto PermissionSet::CanReadKind(std::string_view kind) const -> bool {
  return documents_read && HasAllKinds(document_read_kinds, kind);
}

auto PermissionSet::CanWriteKind(std::string_view kind) const -> bool {
  return documents_write && HasAllKinds(document_write_kinds, kind);
}

auto PermissionSet::CanPublish(std::string_view topic) const -> bool {
  return Contains(events_publish, topic);
}

auto PermissionSet::CanSubscribe(std::string_view topic) const -> bool {
  return Contains(events_subscribe, topic);
}

auto PermissionSet::CanReachHost(std::string_view host) const -> bool {
  return Contains(network_hosts, host);
}

auto PermissionSet::CanUseCredential(std::string_view id) const -> bool {
  return Contains(credential_ids, id);
}

auto ValidateTopic(std::string_view topic) -> bool {
  if (topic.empty() || topic.size() > 200) {
    return false;
  }
  const auto is_lower = [](char ch) { return ch >= 'a' && ch <= 'z'; };
  const auto is_digit = [](char ch) { return ch >= '0' && ch <= '9'; };
  for (const char ch : topic) {
    if (!is_lower(ch) && !is_digit(ch) && ch != '.' && ch != '-') {
      return false;
    }
  }
  const auto version_pos = topic.rfind(".v");
  if (version_pos == std::string_view::npos || version_pos + 2 == topic.size()) {
    return false;
  }
  for (auto i = version_pos + 2; i < topic.size(); ++i) {
    if (!is_digit(topic[i])) {
      return false;
    }
  }
  return topic.front() != '.' && topic.back() != '.' && topic.find("..") == std::string_view::npos;
}

auto ParseManifest(std::string_view json) -> ManifestResult {
  if (json.empty()) {
    return Fail("manifest is empty");
  }
  QJsonParseError parse_error{};
  const auto document =
      QJsonDocument::fromJson(QByteArray(json.data(), static_cast<int>(json.size())), &parse_error);
  if (parse_error.error != QJsonParseError::NoError) {
    return Fail("manifest is not valid JSON: " + parse_error.errorString().toStdString());
  }
  if (!document.isObject()) {
    return Fail("manifest must be a JSON object");
  }
  const auto object = document.object();

  PluginManifest manifest;
  std::string error;
  if (!ReadRequiredString(object, "id", manifest.id, error) ||
      !ReadRequiredString(object, "name", manifest.name, error) ||
      !ReadRequiredString(object, "version", manifest.version, error) ||
      !ReadRequiredString(object, "min_platform_version", manifest.min_platform_version, error)) {
    return Fail(std::move(error));
  }
  if (!IsValidId(manifest.id)) {
    return Fail("manifest id '" + manifest.id + "' must look like org.example.plugin");
  }

  const auto api_version = object.value(QStringLiteral("api_version"));
  if (!api_version.isDouble() || api_version.toInt() != api_version.toDouble()) {
    return Fail("manifest field 'api_version' must be an integer");
  }
  manifest.api_version = api_version.toInt();
  if (manifest.api_version != kPluginApiVersion) {
    return Fail("unsupported api_version " + std::to_string(manifest.api_version) +
                " (this platform implements " + std::to_string(kPluginApiVersion) + ")");
  }

  const auto kind = object.value(QStringLiteral("kind"));
  if (!kind.isString()) {
    return Fail("manifest field 'kind' must be a string");
  }
  const auto parsed_kind = PluginKindFromString(kind.toString().toStdString());
  if (!parsed_kind.has_value()) {
    return Fail("unknown plugin kind '" + kind.toString().toStdString() + "'");
  }
  manifest.kind = *parsed_kind;

  const auto requires_value = object.value(QStringLiteral("requires"));
  if (!requires_value.isUndefined()) {
    if (!ReadStringArray(requires_value, "requires", manifest.dependencies, error)) {
      return Fail(std::move(error));
    }
  }

  if (!ParseContributions(object.value(QStringLiteral("contributes")), manifest.contributes,
                          error)) {
    return Fail(std::move(error));
  }
  if (!ParsePermissions(object.value(QStringLiteral("permissions")), manifest.permissions, error)) {
    return Fail(std::move(error));
  }

  for (const auto& topic : manifest.permissions.events_publish) {
    if (!ValidateTopic(topic)) {
      return Fail("invalid publish topic '" + topic + "' (expected <domain>.<subject>.vN)");
    }
  }
  for (const auto& topic : manifest.permissions.events_subscribe) {
    if (!ValidateTopic(topic)) {
      return Fail("invalid subscribe topic '" + topic + "' (expected <domain>.<subject>.vN)");
    }
  }

  return ManifestResult{std::move(manifest), {}};
}

}  // namespace cppwiki::plugins
