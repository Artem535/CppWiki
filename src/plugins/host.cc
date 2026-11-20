#include "plugins/host.h"

#include <spdlog/spdlog.h>

#include <QUrl>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <deque>
#include <exception>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "plugins/api/ui.h"

namespace cppwiki::plugins {

namespace {

auto IsNativeSlot(const PluginManifest& manifest) -> bool {
  return std::any_of(manifest.contributes.begin(), manifest.contributes.end(),
                     [](const PluginContribution& contribution) {
                       return contribution.surface == SurfaceKind::kNative;
                     });
}

class ScopedDocumentAccess final : public IDocumentAccess {
 public:
  ScopedDocumentAccess(const PermissionSet& permissions, IDocumentAccess* provider,
                       std::string plugin_id, std::vector<AuditRecord>& trail, IAuditLog* sink)
      : permissions_(permissions),
        provider_(provider),
        plugin_id_(std::move(plugin_id)),
        trail_(trail),
        sink_(sink) {}

  auto Read(std::string_view kind, std::string_view id) -> std::optional<std::string> override {
    if (!permissions_.CanReadKind(kind) || provider_ == nullptr) {
      return std::nullopt;
    }
    return provider_->Read(kind, id);
  }

  auto Write(std::string_view kind, std::string_view id, std::string_view body) -> bool override {
    if (!permissions_.CanWriteKind(kind) || provider_ == nullptr) {
      return false;
    }
    const bool written = provider_->Write(kind, id, body);
    Record("document.write", kind, id, written ? "ok" : "failed");
    return written;
  }

  auto List(std::string_view kind) -> std::vector<std::string> override {
    if (!permissions_.CanReadKind(kind) || provider_ == nullptr) {
      return {};
    }
    return provider_->List(kind);
  }

 private:
  void Record(std::string_view operation, std::string_view kind, std::string_view id,
              std::string detail) {
    AuditRecord record;
    record.plugin_id = plugin_id_;
    record.operation = std::string(operation);
    record.document_kind = std::string(kind);
    record.document_id = std::string(id);
    record.detail = std::move(detail);
    trail_.push_back(record);
    if (sink_ != nullptr) {
      sink_->Record(record);
    }
  }

  const PermissionSet& permissions_;
  IDocumentAccess* provider_;
  std::string plugin_id_;
  std::vector<AuditRecord>& trail_;
  IAuditLog* sink_;
};

class ScopedEventBus final : public IEventBus {
 public:
  ScopedEventBus(const PermissionSet& permissions, EventBus& bus, std::string plugin_id)
      : permissions_(permissions), bus_(bus), plugin_id_(std::move(plugin_id)) {}

  ~ScopedEventBus() override {
    for (const auto id : owned_subscriptions_) {
      bus_.Unsubscribe(id);
    }
  }

  auto Publish(std::string_view topic, std::string_view payload_json) -> bool override {
    if (!permissions_.CanPublish(topic)) {
      spdlog::warn("[plugin:{}] denied publish to '{}'", plugin_id_, topic);
      return false;
    }
    bus_.Publish(topic, payload_json);
    return true;
  }

  auto Subscribe(std::string_view topic, EventHandler handler) -> std::uint64_t override {
    if (!permissions_.CanSubscribe(topic)) {
      spdlog::warn("[plugin:{}] denied subscribe to '{}'", plugin_id_, topic);
      return 0;
    }
    const auto id = bus_.Subscribe(topic, std::move(handler));
    if (id != 0) {
      owned_subscriptions_.insert(id);
    }
    return id;
  }

  void Unsubscribe(std::uint64_t subscription_id) override {
    if (owned_subscriptions_.erase(subscription_id) != 0) {
      bus_.Unsubscribe(subscription_id);
    }
  }

 private:
  const PermissionSet& permissions_;
  EventBus& bus_;
  std::string plugin_id_;
  std::set<std::uint64_t> owned_subscriptions_;
};

class ScopedSettings final : public IPluginSettings {
 public:
  ScopedSettings(IPluginSettings* provider, std::string plugin_id)
      : provider_(provider), prefix_("plugin." + plugin_id + ".") {}

  auto Get(std::string_view key) -> std::optional<std::string> override {
    if (provider_ == nullptr) {
      return std::nullopt;
    }
    return provider_->Get(prefix_ + std::string(key));
  }

  auto Set(std::string_view key, std::string_view value) -> bool override {
    if (provider_ == nullptr) {
      return false;
    }
    return provider_->Set(prefix_ + std::string(key), value);
  }

 private:
  IPluginSettings* provider_;
  std::string prefix_;
};

class ScopedStorage final : public IPluginStorage {
 public:
  ScopedStorage(const PermissionSet& permissions, std::filesystem::path root, std::string plugin_id)
      : root_(permissions.filesystem_plugin_data && !root.empty() ? root / plugin_id
                                                                  : std::filesystem::path()) {}

  [[nodiscard]] auto Root() const -> std::filesystem::path override {
    return root_;
  }

 private:
  std::filesystem::path root_;
};

class ScopedModels final : public IModelStore {
 public:
  ScopedModels(const PermissionSet& permissions, IModelStore* provider)
      : allowed_(permissions.models_read), provider_(provider) {}

  auto Resolve(std::string_view name) -> std::optional<std::filesystem::path> override {
    if (!allowed_ || provider_ == nullptr) {
      return std::nullopt;
    }
    return provider_->Resolve(name);
  }

 private:
  bool allowed_;
  IModelStore* provider_;
};

class ScopedNetwork final : public INetworkBroker {
 public:
  ScopedNetwork(const PermissionSet& permissions, INetworkBroker* provider, std::string plugin_id)
      : permissions_(permissions), provider_(provider), plugin_id_(std::move(plugin_id)) {}

  auto Request(const NetworkRequest& request) -> NetworkResponse override {
    const QUrl url(QString::fromStdString(request.url));
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https")) {
      return NetworkResponse{0, {}, "invalid URL"};
    }
    const auto host = url.host().toStdString();
    if (host.empty() || !permissions_.CanReachHost(host)) {
      spdlog::warn("[plugin:{}] denied network access to '{}'", plugin_id_, host);
      return NetworkResponse{0, {}, "host is not allowed by the manifest"};
    }
    if (provider_ == nullptr) {
      return NetworkResponse{0, {}, "network broker is not configured"};
    }
    return provider_->Request(request);
  }

 private:
  const PermissionSet& permissions_;
  INetworkBroker* provider_;
  std::string plugin_id_;
};

class ScopedCredentials final : public ICredentials {
 public:
  ScopedCredentials(const PermissionSet& permissions, ICredentials* provider)
      : permissions_(permissions), provider_(provider) {}

  auto Acquire(std::string_view credential_id) -> std::optional<std::string> override {
    if (!permissions_.CanUseCredential(credential_id) || provider_ == nullptr) {
      return std::nullopt;
    }
    return provider_->Acquire(credential_id);
  }

 private:
  const PermissionSet& permissions_;
  ICredentials* provider_;
};

class ScopedLog final : public IPluginLog {
 public:
  ScopedLog(IPluginLog* provider, std::string plugin_id)
      : provider_(provider), prefix_("[plugin:" + plugin_id + "] ") {}

  void Write(LogLevel level, std::string_view message) override {
    if (provider_ == nullptr) {
      return;
    }
    provider_->Write(level, prefix_ + std::string(message));
  }

 private:
  IPluginLog* provider_;
  std::string prefix_;
};

class ScopedUi final : public IUiHost {
 public:
  ScopedUi(UiRegistry& registry, std::string plugin_id)
      : registry_(registry), plugin_id_(std::move(plugin_id)) {}

  auto UpdateSurface(std::string_view contribution_id, const SurfaceNode& surface)
      -> bool override {
    return registry_.UpdateSurface(plugin_id_, contribution_id, surface);
  }

  auto OnSurfaceEvent(std::string_view contribution_id,
                      std::function<void(const SurfaceEvent&)> handler) -> bool override {
    return registry_.SetSurfaceEventSink(plugin_id_, contribution_id, std::move(handler));
  }

 private:
  UiRegistry& registry_;
  std::string plugin_id_;
};

class ScopedPluginContext final : public IPluginContext {
 public:
  ScopedPluginContext(std::string plugin_id, const PermissionSet& permissions,
                      PlatformServices& services, EventBus& bus, UiRegistry& ui,
                      std::vector<AuditRecord>& audit)
      : plugin_id_(std::move(plugin_id)),
        documents_(permissions, services.documents, plugin_id_, audit, services.audit),
        events_(permissions, bus, plugin_id_),
        settings_(services.settings, plugin_id_),
        storage_(permissions, services.plugin_data_root, plugin_id_),
        models_(permissions, services.models),
        network_(permissions, services.network, plugin_id_),
        credentials_(permissions, services.credentials),
        log_(services.log, plugin_id_),
        ui_(ui, plugin_id_) {}

  [[nodiscard]] auto PluginId() const -> std::string_view override {
    return plugin_id_;
  }
  auto Documents() -> IDocumentAccess& override {
    return documents_;
  }
  auto Events() -> IEventBus& override {
    return events_;
  }
  auto Settings() -> IPluginSettings& override {
    return settings_;
  }
  auto Storage() -> IPluginStorage& override {
    return storage_;
  }
  auto Models() -> IModelStore& override {
    return models_;
  }
  auto Network() -> INetworkBroker& override {
    return network_;
  }
  auto Credentials() -> ICredentials& override {
    return credentials_;
  }
  auto Log() -> IPluginLog& override {
    return log_;
  }
  auto Ui() -> IUiHost& override {
    return ui_;
  }

 private:
  std::string plugin_id_;
  ScopedDocumentAccess documents_;
  ScopedEventBus events_;
  ScopedSettings settings_;
  ScopedStorage storage_;
  ScopedModels models_;
  ScopedNetwork network_;
  ScopedCredentials credentials_;
  ScopedLog log_;
  ScopedUi ui_;
};

}  // namespace

struct PluginHost::Impl {
  struct Entry {
    PluginManifest manifest;
    PluginTier tier = PluginTier::kNative;
    PluginFactory factory;
    std::unique_ptr<IPlugin> instance;
    std::unique_ptr<IPluginContext> context;
    PluginState state = PluginState::kRegistered;
    std::string error;
  };

  Impl(IExecutor& executor_ref, PlatformServices platform_services)
      : executor(&executor_ref), services(std::move(platform_services)), bus(executor_ref) {}

  auto Find(std::string_view id) -> Entry* {
    const auto it = std::find_if(entries.begin(), entries.end(),
                                 [id](const Entry& entry) { return entry.manifest.id == id; });
    return it == entries.end() ? nullptr : &*it;
  }

  auto RunOnExecutor(std::function<void()> work, std::chrono::milliseconds timeout) -> bool {
    if (executor->IsExecutorThread()) {
      work();
      return true;
    }
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();
    executor->Post([promise, work = std::move(work)]() mutable {
      try {
        work();
      } catch (...) {
        // The failure policy lives inside the work; a crash here must not lose the waiter.
      }
      promise->set_value();
    });
    return future.wait_for(timeout) == std::future_status::ready;
  }

  void Fail(Entry& entry, std::string message) {
    entry.state = PluginState::kFailed;
    entry.error = std::move(message);
    spdlog::error("[plugin:{}] {}", entry.manifest.id, entry.error);
    ui.RemovePlugin(entry.manifest.id);
    entry.context.reset();
    entry.instance.reset();
  }

  void LoadOnThisThread() {
    if (entries.empty() || !start_order.empty()) {
      return;
    }
    std::map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < entries.size(); ++i) {
      index.emplace(entries[i].manifest.id, i);
    }
    for (auto& entry : entries) {
      for (const auto& dependency : entry.manifest.dependencies) {
        if (!index.contains(dependency)) {
          Fail(entry, "missing dependency '" + dependency + "'");
          break;
        }
      }
    }

    std::map<std::string, std::size_t> indegree;
    std::map<std::string, std::vector<std::string>> dependents;
    for (const auto& entry : entries) {
      indegree.emplace(entry.manifest.id, entry.manifest.dependencies.size());
      for (const auto& dependency : entry.manifest.dependencies) {
        dependents[dependency].push_back(entry.manifest.id);
      }
    }
    std::deque<std::string> ready;
    for (const auto& entry : entries) {
      if (indegree[entry.manifest.id] == 0) {
        ready.push_back(entry.manifest.id);
      }
    }
    std::vector<std::string> order;
    while (!ready.empty()) {
      const auto id = ready.front();
      ready.pop_front();
      order.push_back(id);
      for (const auto& dependent : dependents[id]) {
        auto& remaining = indegree[dependent];
        if (remaining > 0) {
          --remaining;
          if (remaining == 0) {
            ready.push_back(dependent);
          }
        }
      }
    }
    if (order.size() != entries.size()) {
      for (const auto& entry : entries) {
        if (std::find(order.begin(), order.end(), entry.manifest.id) == order.end() &&
            entry.state != PluginState::kFailed) {
          auto* mutable_entry = Find(entry.manifest.id);
          Fail(*mutable_entry, "dependency cycle");
        }
      }
    }

    for (const auto& id : order) {
      auto* entry = Find(id);
      if (entry == nullptr || entry->state == PluginState::kFailed) {
        continue;
      }
      try {
        entry->instance = entry->factory();
      } catch (const std::exception& error) {
        Fail(*entry, std::string("factory threw: ") + error.what());
        continue;
      } catch (...) {
        Fail(*entry, "factory threw an unknown exception");
        continue;
      }
      if (!entry->instance) {
        Fail(*entry, "factory returned null");
        continue;
      }
      const bool native_needed = IsNativeSlot(entry->manifest);
      if (native_needed && dynamic_cast<IViewPlugin*>(entry->instance.get()) == nullptr) {
        Fail(*entry, "native contributions require an IViewPlugin implementation");
        continue;
      }
      std::function<QWidget*(IViewContext&)> native_factory;
      if (native_needed) {
        const auto plugin_id = entry->manifest.id;
        native_factory = [this, plugin_id](IViewContext& context) -> QWidget* {
          auto* target = Find(plugin_id);
          if (target == nullptr || !target->instance) {
            return nullptr;
          }
          auto* view_plugin = dynamic_cast<IViewPlugin*>(target->instance.get());
          return view_plugin == nullptr ? nullptr : view_plugin->CreateView(context);
        };
      }
      const auto error = ui.Register(entry->manifest, entry->tier, std::move(native_factory));
      if (!error.empty()) {
        Fail(*entry, error);
        continue;
      }
      entry->state = PluginState::kLoaded;
    }
    start_order = std::move(order);
  }

  void StartOnThisThread() {
    for (const auto& id : start_order) {
      auto* entry = Find(id);
      if (entry == nullptr || entry->state != PluginState::kLoaded) {
        continue;
      }
      bool dependency_ready = true;
      for (const auto& dependency : entry->manifest.dependencies) {
        auto* dependency_entry = Find(dependency);
        if (dependency_entry == nullptr || dependency_entry->state != PluginState::kStarted) {
          dependency_ready = false;
          Fail(*entry, "dependency '" + dependency + "' did not start");
          break;
        }
      }
      if (!dependency_ready) {
        continue;
      }
      entry->context = std::make_unique<ScopedPluginContext>(
          entry->manifest.id, entry->manifest.permissions, services, bus, ui, audit);
      bool started = false;
      try {
        started = entry->instance->Start(*entry->context);
      } catch (const std::exception& error) {
        Fail(*entry, std::string("start threw: ") + error.what());
        continue;
      } catch (...) {
        Fail(*entry, "start threw an unknown exception");
        continue;
      }
      if (!started) {
        Fail(*entry, "start returned false");
        continue;
      }
      entry->state = PluginState::kStarted;
    }
  }

  void StopOnThisThread() {
    for (auto it = start_order.rbegin(); it != start_order.rend(); ++it) {
      auto* entry = Find(*it);
      if (entry == nullptr || entry->state != PluginState::kStarted) {
        continue;
      }
      try {
        entry->instance->Stop();
      } catch (const std::exception& error) {
        entry->error = std::string("stop threw: ") + error.what();
        spdlog::error("[plugin:{}] {}", entry->manifest.id, entry->error);
      } catch (...) {
        entry->error = "stop threw an unknown exception";
        spdlog::error("[plugin:{}] {}", entry->manifest.id, entry->error);
      }
      entry->state = PluginState::kStopped;
      ui.RemovePlugin(entry->manifest.id);
      entry->context.reset();
      entry->instance.reset();
    }
  }

  IExecutor* executor;
  PlatformServices services;
  EventBus bus;
  UiRegistry ui;
  std::vector<AuditRecord> audit;
  std::vector<Entry> entries;
  std::vector<std::string> start_order;
};

PluginHost::PluginHost(IExecutor& executor, PlatformServices services)
    : impl_(std::make_unique<Impl>(executor, std::move(services))) {}

PluginHost::~PluginHost() = default;

auto PluginHost::Register(std::string_view manifest_json, PluginTier tier, PluginFactory factory)
    -> std::string {
  auto parsed = ParseManifest(manifest_json);
  if (!parsed.manifest.has_value()) {
    return parsed.error;
  }
  auto manifest = std::move(*parsed.manifest);
  if (impl_->Find(manifest.id) != nullptr) {
    return "plugin '" + manifest.id + "' is already registered";
  }
  if (!factory) {
    return "plugin '" + manifest.id + "' has no factory";
  }
  const auto error = impl_->ui.Validate(manifest, tier, true);
  if (!error.empty()) {
    return error;
  }
  Impl::Entry entry;
  entry.manifest = std::move(manifest);
  entry.tier = tier;
  entry.factory = std::move(factory);
  impl_->entries.push_back(std::move(entry));
  return {};
}

void PluginHost::LoadAll() {
  if (!impl_->RunOnExecutor([this] { impl_->LoadOnThisThread(); }, std::chrono::seconds(30))) {
    spdlog::error("plugin load timed out");
  }
}

void PluginHost::StartAll() {
  const bool needs_load =
      std::any_of(impl_->entries.begin(), impl_->entries.end(),
                  [](const Impl::Entry& entry) { return entry.state == PluginState::kRegistered; });
  if (needs_load) {
    LoadAll();
  }
  if (!impl_->RunOnExecutor([this] { impl_->StartOnThisThread(); }, std::chrono::seconds(30))) {
    spdlog::error("plugin start timed out");
  }
}

void PluginHost::StopAll(std::chrono::milliseconds deadline) {
  if (!impl_->RunOnExecutor([this] { impl_->StopOnThisThread(); }, deadline)) {
    spdlog::error("plugin stop did not finish within {} ms", deadline.count());
  }
}

auto PluginHost::State(std::string_view id) const -> PluginState {
  const auto it = std::find_if(impl_->entries.begin(), impl_->entries.end(),
                               [id](const Impl::Entry& entry) { return entry.manifest.id == id; });
  return it == impl_->entries.end() ? PluginState::kFailed : it->state;
}

auto PluginHost::Error(std::string_view id) const -> std::string {
  const auto it = std::find_if(impl_->entries.begin(), impl_->entries.end(),
                               [id](const Impl::Entry& entry) { return entry.manifest.id == id; });
  return it == impl_->entries.end() ? std::string("unknown plugin") : it->error;
}

auto PluginHost::Snapshots() const -> std::vector<PluginSnapshot> {
  std::vector<PluginSnapshot> snapshots;
  snapshots.reserve(impl_->entries.size());
  for (const auto& entry : impl_->entries) {
    snapshots.push_back(
        PluginSnapshot{.id = entry.manifest.id, .state = entry.state, .error = entry.error});
  }
  return snapshots;
}

auto PluginHost::Ui() -> UiRegistry& {
  return impl_->ui;
}

auto PluginHost::Bus() -> EventBus& {
  return impl_->bus;
}

auto PluginHost::AuditTrail() const -> const std::vector<AuditRecord>& {
  return impl_->audit;
}

}  // namespace cppwiki::plugins
