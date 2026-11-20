#ifndef CPPWIKI_SRC_PLUGINS_API_SERVICES_H_
#define CPPWIKI_SRC_PLUGINS_API_SERVICES_H_

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cppwiki::plugins {

struct SurfaceNode;
struct SurfaceEvent;

using EventHandler = std::function<void(std::string_view payload_json)>;

// Scoped document access. Every call names the document kind so the host can enforce the
// manifest's kind filter before touching a repository.
class IDocumentAccess {
 public:
  virtual ~IDocumentAccess() = default;
  virtual auto Read(std::string_view kind, std::string_view id) -> std::optional<std::string> = 0;
  virtual auto Write(std::string_view kind, std::string_view id, std::string_view body) -> bool = 0;
  virtual auto List(std::string_view kind) -> std::vector<std::string> = 0;
};

class IEventBus {
 public:
  virtual ~IEventBus() = default;
  virtual auto Publish(std::string_view topic, std::string_view payload_json) -> bool = 0;
  virtual auto Subscribe(std::string_view topic, EventHandler handler) -> std::uint64_t = 0;
  virtual void Unsubscribe(std::uint64_t subscription_id) = 0;
};

class IPluginSettings {
 public:
  virtual ~IPluginSettings() = default;
  virtual auto Get(std::string_view key) -> std::optional<std::string> = 0;
  virtual auto Set(std::string_view key, std::string_view value) -> bool = 0;
};

class IPluginStorage {
 public:
  virtual ~IPluginStorage() = default;
  [[nodiscard]] virtual auto Root() const -> std::filesystem::path = 0;
};

class IModelStore {
 public:
  virtual ~IModelStore() = default;
  virtual auto Resolve(std::string_view name) -> std::optional<std::filesystem::path> = 0;
};

struct NetworkRequest {
  std::string url;
  std::string method = "GET";
  std::string body;
  std::map<std::string, std::string> headers;
};

struct NetworkResponse {
  int status = 0;
  std::string body;
  std::string error;
};

class INetworkBroker {
 public:
  virtual ~INetworkBroker() = default;
  virtual auto Request(const NetworkRequest& request) -> NetworkResponse = 0;
};

class ICredentials {
 public:
  virtual ~ICredentials() = default;
  virtual auto Acquire(std::string_view credential_id) -> std::optional<std::string> = 0;
};

enum class LogLevel { kDebug, kInfo, kWarn, kError };

class IPluginLog {
 public:
  virtual ~IPluginLog() = default;
  virtual void Write(LogLevel level, std::string_view message) = 0;
};

// Display-side facade: a plugin publishes a declarative surface for a contribution it owns,
// and receives host-rendered interactions back through the handler.
class IUiHost {
 public:
  virtual ~IUiHost() = default;
  virtual auto UpdateSurface(std::string_view contribution_id, const SurfaceNode& surface)
      -> bool = 0;
  virtual auto OnSurfaceEvent(std::string_view contribution_id,
                              std::function<void(const SurfaceEvent&)> handler) -> bool = 0;
};

class IPluginContext {
 public:
  virtual ~IPluginContext() = default;

  [[nodiscard]] virtual auto PluginId() const -> std::string_view = 0;
  virtual auto Documents() -> IDocumentAccess& = 0;
  virtual auto Events() -> IEventBus& = 0;
  virtual auto Settings() -> IPluginSettings& = 0;
  virtual auto Storage() -> IPluginStorage& = 0;
  virtual auto Models() -> IModelStore& = 0;
  virtual auto Network() -> INetworkBroker& = 0;
  virtual auto Credentials() -> ICredentials& = 0;
  virtual auto Log() -> IPluginLog& = 0;
  virtual auto Ui() -> IUiHost& = 0;
};

struct AuditRecord {
  std::string plugin_id;
  std::string operation;
  std::string document_kind;
  std::string document_id;
  std::string detail;
};

class IAuditLog {
 public:
  virtual ~IAuditLog() = default;
  virtual void Record(const AuditRecord& record) = 0;
};

}  // namespace cppwiki::plugins

#endif  // CPPWIKI_SRC_PLUGINS_API_SERVICES_H_
