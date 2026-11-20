#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "plugins/api/plugin.h"
#include "plugins/api/services.h"
#include "plugins/api/ui.h"
#include "plugins/display_registry.h"
#include "plugins/event_bus.h"
#include "plugins/executor.h"
#include "plugins/host.h"
#include "plugins/manifest.h"
#include "plugins/surface.h"

namespace {

using namespace cppwiki::plugins;

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto RequireContains(std::string_view haystack, std::string_view needle, std::string_view test)
    -> void {
  if (haystack.find(needle) == std::string_view::npos) {
    spdlog::error("FAIL: {} (expected '{}' in '{}')", test, needle, haystack);
    std::exit(EXIT_FAILURE);
  }
}

auto ParseOk(std::string_view json) -> PluginManifest {
  auto result = ParseManifest(json);
  Require(result.manifest.has_value(), "manifest should parse: " + result.error);
  return std::move(*result.manifest);
}

auto RequireManifestError(std::string_view json, std::string_view needle, std::string_view test)
    -> void {
  const auto result = ParseManifest(json);
  Require(!result.manifest.has_value(), std::string("manifest should fail: ") + std::string(test));
  RequireContains(result.error, needle, test);
}

constexpr std::string_view kValidManifest = R"json(
{
  "id": "org.example.search",
  "name": "Search",
  "version": "0.1.0",
  "api_version": 1,
  "min_platform_version": "0.4.0",
  "kind": "search-provider",
  "contributes": [
    { "slot": "search.results", "id": "results", "title": "Wiki", "surface": "declarative" }
  ],
  "permissions": {
    "documents": { "read": ["wikiPage"], "write": ["task"] },
    "attachments": { "read": true, "write": false },
    "filesystem": ["plugin-data"],
    "models": ["read"],
    "network": ["outline.example.com"],
    "credentials": ["outline-token"],
    "events": {
      "publish": ["search.hits.v1"],
      "subscribe": ["meeting.transcript.phrase.v1"]
    }
  }
}
)json";

auto MakeServiceManifest(std::string_view id, std::string_view requires_json = "[]")
    -> std::string {
  return std::string(R"json({
  "id": ")json") +
         std::string(id) + R"json(",
  "name": "Service )json" +
         std::string(id) + R"json(",
  "version": "0.1.0",
  "api_version": 1,
  "min_platform_version": "0.4.0",
  "kind": "service",
  "requires": )json" +
         std::string(requires_json) + R"json(,
  "permissions": {}
})json";
}

class FakeDocumentStore final : public IDocumentAccess {
 public:
  auto Read(std::string_view kind, std::string_view id) -> std::optional<std::string> override {
    const auto it = documents.find({std::string(kind), std::string(id)});
    return it == documents.end() ? std::nullopt : std::optional<std::string>(it->second);
  }

  auto Write(std::string_view kind, std::string_view id, std::string_view body) -> bool override {
    documents[{std::string(kind), std::string(id)}] = std::string(body);
    return true;
  }

  auto List(std::string_view kind) -> std::vector<std::string> override {
    std::vector<std::string> ids;
    for (const auto& [key, value] : documents) {
      (void)value;
      if (key.first == kind) {
        ids.push_back(key.second);
      }
    }
    return ids;
  }

  std::map<std::pair<std::string, std::string>, std::string> documents;
};

class FakeSettings final : public IPluginSettings {
 public:
  auto Get(std::string_view key) -> std::optional<std::string> override {
    const auto it = values.find(std::string(key));
    return it == values.end() ? std::nullopt : std::optional<std::string>(it->second);
  }

  auto Set(std::string_view key, std::string_view value) -> bool override {
    values[std::string(key)] = std::string(value);
    return true;
  }

  std::map<std::string, std::string> values;
};

class FakeCredentials final : public ICredentials {
 public:
  auto Acquire(std::string_view credential_id) -> std::optional<std::string> override {
    return credential_id == "outline-token" ? std::optional<std::string>("secret") : std::nullopt;
  }
};

class RecordingAudit final : public IAuditLog {
 public:
  void Record(const AuditRecord& record) override {
    records.push_back(record);
  }
  std::vector<AuditRecord> records;
};

struct PluginBehavior {
  std::function<bool(IPluginContext&)> start;
  std::function<void()> stop;
  std::function<void()> stop_sleep;
};

class LambdaPlugin final : public IPlugin {
 public:
  explicit LambdaPlugin(PluginBehavior behavior) : behavior_(std::move(behavior)) {}

  auto Start(IPluginContext& context) -> bool override {
    return behavior_.start ? behavior_.start(context) : true;
  }

  void Stop() override {
    if (behavior_.stop) {
      behavior_.stop();
    }
    if (behavior_.stop_sleep) {
      behavior_.stop_sleep();
    }
  }

 private:
  PluginBehavior behavior_;
};

auto MakeFactory(PluginBehavior behavior) -> PluginFactory {
  return [behavior = std::move(behavior)]() -> std::unique_ptr<IPlugin> {
    return std::make_unique<LambdaPlugin>(behavior);
  };
}

class FakeViewContext final : public IViewContext {
 public:
  [[nodiscard]] auto PluginId() const -> std::string_view override {
    return plugin_id;
  }
  [[nodiscard]] auto ContributionId() const -> std::string_view override {
    return contribution_id;
  }
  [[nodiscard]] auto ThemeToken(std::string_view name) const -> std::string override {
    return name == "text" ? "rgb(255,255,255)" : std::string();
  }
  auto RunCommand(std::string_view command, std::string_view argument) -> bool override {
    (void)command;
    (void)argument;
    return false;
  }
  auto SetSurface(const SurfaceNode&) -> bool override {
    return false;
  }

  std::string plugin_id;
  std::string contribution_id;
};

auto TestManifestParsing() -> void {
  const auto manifest = ParseOk(kValidManifest);
  Require(manifest.id == "org.example.search", "manifest id");
  Require(manifest.kind == PluginKind::kSearchProvider, "manifest kind");
  Require(manifest.api_version == kPluginApiVersion, "manifest api version");
  Require(manifest.permissions.documents_read, "documents read granted");
  Require(manifest.permissions.CanReadKind("wikiPage"), "wikiPage readable");
  Require(!manifest.permissions.CanReadKind("task"), "task is not readable");
  Require(manifest.permissions.CanWriteKind("task"), "task writable");
  Require(!manifest.permissions.CanWriteKind("wikiPage"), "wikiPage is not writable");
  Require(manifest.permissions.CanPublish("search.hits.v1"), "publish topic");
  Require(!manifest.permissions.CanPublish("search.other.v1"), "undeclared publish denied");
  Require(manifest.permissions.CanSubscribe("meeting.transcript.phrase.v1"), "subscribe topic");
  Require(manifest.permissions.CanReachHost("outline.example.com"), "network host");
  Require(!manifest.permissions.CanReachHost("evil.example.com"), "other host denied");
  Require(manifest.permissions.CanUseCredential("outline-token"), "credential");
  Require(!manifest.permissions.CanUseCredential("github-token"), "other credential denied");
  Require(manifest.contributes.size() == 1, "contribution count");
  Require(manifest.contributes.front().surface == SurfaceKind::kDeclarative, "surface kind");

  RequireManifestError("", "empty", "empty manifest");
  RequireManifestError(R"json({"id": "x"})json", "must be a non-empty string", "missing fields");
  RequireManifestError(R"json({"id": "org.example.test", "name": "T", "version": "1",
    "min_platform_version": "0.1", "api_version": 2, "kind": "service"})json",
                       "unsupported api_version", "api version mismatch");
  RequireManifestError(R"json({"id": "org.example.test", "name": "T", "version": "1",
    "min_platform_version": "0.1", "api_version": 1, "kind": "nope"})json",
                       "unknown plugin kind", "unknown kind");
  RequireManifestError(R"json({"id": "bad id", "name": "T", "version": "1",
    "min_platform_version": "0.1", "api_version": 1, "kind": "service"})json",
                       "must look like", "invalid id");
  RequireManifestError(R"json({"id": "org.example.test", "name": "T", "version": "1",
    "min_platform_version": "0.1", "api_version": 1, "kind": "service",
    "permissions": {"admin": true}})json",
                       "unknown permission", "unknown permission");
  RequireManifestError(R"json({"id": "org.example.test", "name": "T", "version": "1",
    "min_platform_version": "0.1", "api_version": 1, "kind": "service",
    "permissions": {"events": {"publish": ["NotATopic"]}}})json",
                       "invalid publish topic", "invalid topic");
  RequireManifestError(R"json({"id": "org.example.test", "name": "T", "version": "1",
    "min_platform_version": "0.1", "api_version": 1, "kind": "service",
    "contributes": [{"slot": "search.results", "id": "r"},
                    {"slot": "search.results", "id": "r"}]})json",
                       "duplicate contribution", "duplicate contribution id");
}

auto TestTopics() -> void {
  Require(ValidateTopic("search.hits.v1"), "valid topic");
  Require(ValidateTopic("meeting.transcript.phrase.v12"), "multi-digit version");
  Require(!ValidateTopic("Search.Hits.v1"), "uppercase rejected");
  Require(!ValidateTopic("search.hits"), "missing version rejected");
  Require(!ValidateTopic("search..hits.v1"), "empty segment rejected");
  Require(!ValidateTopic(".search.v1"), "leading dot rejected");
  Require(!ValidateTopic("search.v1x"), "trailing garbage rejected");
}

auto TestSurfaceParsing() -> void {
  constexpr std::string_view kSurface = R"json(
{
  "surface_version": 1,
  "type": "panel",
  "children": [
    { "type": "section", "title": "Outline", "children": [
      { "type": "key-value", "items": [["Last sync", "2 minutes ago"]] },
      { "type": "progress", "value": 0.4, "label": "Indexing" }
    ]},
    { "type": "actions", "items": [
      { "id": "sync-now", "label": "Sync now", "style": "primary" }
    ]}
  ]
}
)json";
  auto parsed = ParseSurface(kSurface);
  Require(parsed.root.has_value(), "surface parses: " + parsed.error);
  Require(parsed.root->type == "panel", "root is panel");
  Require(parsed.root->children.size() == 2, "panel children");
  Require(parsed.root->children[0].children[0].key_values.size() == 1, "key value item");
  Require(parsed.root->children[1].actions.size() == 1, "action item");

  auto unknown = ParseSurface(R"json({"surface_version":1,"type":"html"})json");
  Require(!unknown.root.has_value(), "unknown primitive rejected");
  RequireContains(unknown.error, "unknown surface primitive", "unknown primitive error");

  auto url =
      ParseSurface(R"json({"surface_version":1,"type":"text","text":"https://example.com"})json");
  Require(!url.root.has_value(), "URL rejected");

  auto wrong_version = ParseSurface(R"json({"surface_version":2,"type":"panel"})json");
  Require(!wrong_version.root.has_value(), "wrong version rejected");

  auto table = ParseSurface(R"json({"surface_version":1,"type":"table",
      "columns":["A","B"],"rows":[["1"]]})json");
  Require(!table.root.has_value(), "table row width rejected");

  auto progress = ParseSurface(R"json({"surface_version":1,"type":"progress","value":1.5})json");
  Require(!progress.root.has_value(), "progress out of range rejected");

  std::string deep = R"json({"surface_version":1,"type":"panel","children":[)json";
  constexpr int kDepth = 10;
  for (int i = 0; i < kDepth; ++i) {
    deep += R"json({"type":"panel","children":[)json";
  }
  deep += R"json({"type":"text","text":"deep"})json";
  for (int i = 0; i < kDepth; ++i) {
    deep += "]}";
  }
  deep += "]}";
  auto too_deep = ParseSurface(deep);
  Require(!too_deep.root.has_value(), "depth limit rejected");
  RequireContains(too_deep.error, "depth", "depth limit error");

  std::string wide = R"json({"surface_version":1,"type":"panel","children":[)json";
  for (std::size_t i = 0; i < kMaxSurfaceNodes + 1; ++i) {
    if (i != 0) {
      wide += ",";
    }
    wide += R"json({"type":"text","text":"x"})json";
  }
  wide += "]}";
  auto too_wide = ParseSurface(wide);
  Require(!too_wide.root.has_value(), "node limit rejected");
  RequireContains(too_wide.error, "nodes", "node limit error");
}

auto TestEventBus() -> void {
  ManualExecutor executor;
  EventBus bus(executor);
  std::vector<std::string> received;
  const auto id = bus.Subscribe(
      "search.hits.v1", [&received](std::string_view payload) { received.emplace_back(payload); });
  Require(id != 0, "subscribe returns id");
  Require(bus.Subscribe("bad topic", [](std::string_view) {}) == 0, "invalid topic rejected");

  Require(bus.Publish("search.hits.v1", "one") == 1, "publish queued");
  Require(received.empty(), "delivery is asynchronous");
  executor.RunAll();
  Require(received.size() == 1 && received.front() == "one", "event delivered");

  bus.Unsubscribe(id);
  Require(bus.Publish("search.hits.v1", "two") == 0, "unsubscribed topic has no receivers");
  executor.RunAll();
  Require(received.size() == 1, "no delivery after unsubscribe");

  Require(bus.Publish("bad topic", "x") == 0, "publish invalid topic");
  Require(bus.InvalidTopicCount() == 1, "invalid topic counted");

  ManualExecutor drop_executor;
  EventBus dropping_bus(drop_executor);
  std::vector<std::string> dropped_received;
  dropping_bus.Subscribe(
      "search.hits.v1",
      [&dropped_received](std::string_view payload) { dropped_received.emplace_back(payload); },
      SubscriptionOptions{.max_queue = 2, .drop_newest = false});
  dropping_bus.Publish("search.hits.v1", "a");
  dropping_bus.Publish("search.hits.v1", "b");
  dropping_bus.Publish("search.hits.v1", "c");
  drop_executor.RunAll();
  Require(dropped_received.size() == 2, "bounded queue delivers max_queue items");
  Require(dropped_received[0] == "b" && dropped_received[1] == "c", "oldest item dropped");
  Require(dropping_bus.DroppedCount() == 1, "drop counted");
}

auto TestUiRegistry() -> void {
  auto manifest = ParseOk(kValidManifest);
  manifest.contributes.push_back(PluginContribution{.id = "panel",
                                                    .slot = "page.side-panel",
                                                    .title = "Panel",
                                                    .surface = SurfaceKind::kNative,
                                                    .document_kind = {}});
  manifest.contributes.push_back(PluginContribution{.id = "status",
                                                    .slot = "status.item",
                                                    .title = "Status",
                                                    .surface = SurfaceKind::kDeclarative,
                                                    .document_kind = {}});
  UiRegistry registry;
  auto factory = [](IViewContext&) -> QWidget* {
    return reinterpret_cast<QWidget*>(static_cast<std::uintptr_t>(1));
  };
  Require(registry.Register(manifest, PluginTier::kNative, factory).empty(), "registry accepts");

  Require(registry.Contributions("search.results").size() == 1, "search results registered");
  Require(registry.Contributions("page.side-panel").size() == 1, "side panel registered");

  FakeViewContext context;
  context.plugin_id = manifest.id;
  context.contribution_id = "panel";
  Require(registry.Activate(manifest.id, "panel", context), "native view activates");
  Require(registry.IsActive(manifest.id, "panel"), "native view active");
  Require(!registry.Activate(manifest.id, "results", context), "declarative cannot activate");
  registry.Deactivate(manifest.id, "panel");
  Require(!registry.IsActive(manifest.id, "panel"), "native view deactivated");

  SurfaceNode node;
  node.type = "panel";
  SurfaceNode hello;
  hello.type = "text";
  hello.text = "hello";
  node.children.push_back(std::move(hello));
  Require(registry.UpdateSurface(manifest.id, "results", node), "declarative surface stored");
  Require(registry.Surface(manifest.id, "results") != nullptr, "surface readable");

  SurfaceNode invalid;
  invalid.type = "html";
  Require(!registry.UpdateSurface(manifest.id, "results", invalid), "invalid surface rejected");

  bool observed = false;
  registry.SetSurfaceObserver(
      [&observed](const std::string&, const std::string&, const SurfaceNode&) { observed = true; });
  Require(registry.UpdateSurface(manifest.id, "results", node), "surface update");
  Require(observed, "surface observer notified");

  std::string action;
  Require(registry.SetSurfaceEventSink(
              manifest.id, "results",
              [&action](const SurfaceEvent& event) { action = event.action_id; }),
          "event sink registered");
  Require(registry.Dispatch(
              manifest.id,
              SurfaceEvent{.surface_id = "results", .action_id = "sync-now", .values = {}}),
          "event dispatched");
  Require(action == "sync-now", "event reached the sink");

  // Tier rules and slot validation.
  UiRegistry wasm_registry;
  RequireContains(wasm_registry.Register(manifest, PluginTier::kWasm, factory), "requires a native",
                  "wasm native contribution rejected");

  auto bad_slot = ParseOk(kValidManifest);
  bad_slot.contributes.front().slot = "nonexistent.slot";
  UiRegistry slot_registry;
  RequireContains(slot_registry.Register(bad_slot, PluginTier::kNative, factory), "unknown slot",
                  "unknown slot rejected");

  auto native_status = ParseOk(kValidManifest);
  native_status.contributes.push_back(PluginContribution{.id = "status",
                                                         .slot = "status.item",
                                                         .title = "Status",
                                                         .surface = SurfaceKind::kNative,
                                                         .document_kind = {}});
  UiRegistry status_registry;
  RequireContains(status_registry.Register(native_status, PluginTier::kNative, factory),
                  "does not accept native", "native status item rejected");

  auto editor = ParseOk(kValidManifest);
  editor.contributes.push_back(PluginContribution{.id = "editor",
                                                  .slot = "document.editor",
                                                  .title = "Editor",
                                                  .surface = SurfaceKind::kNative,
                                                  .document_kind = "meetingRecord"});
  UiRegistry editor_registry;
  Require(editor_registry.Register(editor, PluginTier::kNative, factory).empty(),
          "document editor registers");
  auto second_editor = editor;
  second_editor.id = "org.example.other";
  RequireContains(editor_registry.Register(second_editor, PluginTier::kNative, factory),
                  "already has an editor", "duplicate document kind rejected");
}

auto TestHostLifecycle() -> void {
  InlineExecutor executor;
  FakeDocumentStore documents;
  FakeSettings settings;
  FakeCredentials credentials;
  RecordingAudit audit;
  PlatformServices services;
  services.documents = &documents;
  services.settings = &settings;
  services.credentials = &credentials;
  services.audit = &audit;
  services.plugin_data_root = std::filesystem::temp_directory_path() / "cppwiki-plugin-test";

  PluginHost host(executor, services);

  std::vector<std::string> order;
  std::vector<std::string> events;
  bool denied_publish = true;
  bool denied_subscribe = true;
  bool denied_read = true;
  bool write_allowed = false;

  constexpr std::string_view kManifest = R"json({
    "id": "org.example.service",
    "name": "Service",
    "version": "0.1.0",
    "api_version": 1,
    "min_platform_version": "0.4.0",
    "kind": "service",
    "requires": ["org.example.base"],
    "permissions": {
      "documents": { "read": ["wikiPage"], "write": ["task"] },
      "events": {
        "publish": ["meeting.call.state.v1"],
        "subscribe": ["meeting.call.state.v1"]
      },
      "credentials": ["outline-token"],
      "filesystem": ["plugin-data"]
    }
  })json";

  PluginBehavior behavior;
  behavior.start = [&](IPluginContext& context) {
    order.push_back("start:service");
    context.Events().Subscribe("meeting.call.state.v1", [&events](std::string_view payload) {
      events.emplace_back(payload);
    });
    Require(context.Events().Publish("meeting.call.state.v1", "hello"), "declared publish works");
    denied_publish = !context.Events().Publish("search.hits.v1", "nope");
    denied_subscribe = context.Events().Subscribe("search.hits.v1", [](std::string_view) {}) == 0;
    denied_read = !context.Documents().Read("task", "doc").has_value();
    write_allowed = context.Documents().Write("task", "doc", "body");
    Require(context.Credentials().Acquire("outline-token").has_value(), "declared credential");
    Require(!context.Credentials().Acquire("github-token").has_value(), "other credential denied");
    Require(context.Storage().Root().filename() == "org.example.service", "plugin data root");
    Require(!context.Storage().Root().empty(), "plugin data root granted");
    return true;
  };
  behavior.stop = [&] { order.push_back("stop:service"); };

  PluginBehavior base_behavior;
  base_behavior.start = [&](IPluginContext&) {
    order.push_back("start:base");
    return true;
  };
  base_behavior.stop = [&] { order.push_back("stop:base"); };
  Require(host.Register(MakeServiceManifest("org.example.base"), PluginTier::kNative,
                        MakeFactory(base_behavior))
              .empty(),
          "base registers");
  Require(host.Register(kManifest, PluginTier::kNative, MakeFactory(behavior)).empty(),
          "service registers");
  Require(!host.Register(kManifest, PluginTier::kNative, MakeFactory(behavior)).empty(),
          "duplicate registration rejected");

  host.LoadAll();
  host.StartAll();
  Require(host.State("org.example.base") == PluginState::kStarted, "base started");
  Require(host.State("org.example.service") == PluginState::kStarted, "service started");
  Require(denied_publish, "undeclared publish denied");
  Require(denied_subscribe, "undeclared subscribe denied");
  Require(denied_read, "undeclared document kind denied");
  Require(write_allowed, "declared document write allowed");
  Require(!host.AuditTrail().empty(), "document write audited");
  Require(!audit.records.empty(), "audit sink receives records");
  Require(audit.records.front().plugin_id == "org.example.service", "audit plugin id");
  Require(events.size() == 1 && events.front() == "hello", "event delivered");

  host.StopAll(std::chrono::seconds(2));
  Require(host.State("org.example.service") == PluginState::kStopped, "service stopped");
  Require(host.State("org.example.base") == PluginState::kStopped, "base stopped");
  Require(order.size() >= 4, "start/stop order recorded");
  Require(order.front() == "start:base", "dependency starts first");
  Require(order.back() == "stop:base", "dependency stops last");
}

auto TestHostFailures() -> void {
  InlineExecutor executor;
  PlatformServices services;
  PluginHost host(executor, services);

  Require(host.Register(MakeServiceManifest("org.example.a", R"json(["org.example.missing"])json"),
                        PluginTier::kNative, MakeFactory(PluginBehavior{}))
              .empty(),
          "missing-dependency plugin registers");
  Require(host.Register(MakeServiceManifest("org.example.b"), PluginTier::kNative,
                        MakeFactory(PluginBehavior{}))
              .empty(),
          "independent plugin registers");
  host.LoadAll();
  host.StartAll();
  RequireContains(host.Error("org.example.a"), "missing dependency", "missing dependency error");
  Require(host.State("org.example.a") == PluginState::kFailed, "missing dependency failed");
  Require(host.State("org.example.b") == PluginState::kStarted, "independent plugin started");

  InlineExecutor cycle_executor;
  PluginHost cycle_host(cycle_executor, PlatformServices{});
  Require(cycle_host
              .Register(MakeServiceManifest("org.example.x", R"json(["org.example.y"])json"),
                        PluginTier::kNative, MakeFactory(PluginBehavior{}))
              .empty(),
          "cycle x registers");
  Require(cycle_host
              .Register(MakeServiceManifest("org.example.y", R"json(["org.example.x"])json"),
                        PluginTier::kNative, MakeFactory(PluginBehavior{}))
              .empty(),
          "cycle y registers");
  cycle_host.LoadAll();
  cycle_host.StartAll();
  RequireContains(cycle_host.Error("org.example.x"), "cycle", "cycle detected x");
  RequireContains(cycle_host.Error("org.example.y"), "cycle", "cycle detected y");

  InlineExecutor failure_executor;
  PluginHost failure_host(failure_executor, PlatformServices{});
  PluginBehavior failing;
  failing.start = [](IPluginContext&) { return false; };
  Require(failure_host
              .Register(MakeServiceManifest("org.example.fail"), PluginTier::kNative,
                        MakeFactory(failing))
              .empty(),
          "failing plugin registers");
  Require(failure_host
              .Register(MakeServiceManifest("org.example.ok"), PluginTier::kNative,
                        MakeFactory(PluginBehavior{}))
              .empty(),
          "second plugin registers");
  Require(
      failure_host
          .Register(MakeServiceManifest("org.example.dependent", R"json(["org.example.fail"])json"),
                    PluginTier::kNative, MakeFactory(PluginBehavior{}))
          .empty(),
      "dependent plugin registers");
  failure_host.LoadAll();
  failure_host.StartAll();
  Require(failure_host.State("org.example.fail") == PluginState::kFailed, "failed start isolated");
  Require(failure_host.State("org.example.ok") == PluginState::kStarted, "other plugin starts");
  RequireContains(failure_host.Error("org.example.dependent"), "did not start",
                  "dependent of failed plugin fails");
}

auto TestHostDisplay() -> void {
  InlineExecutor executor;
  PlatformServices services;
  PluginHost host(executor, services);

  constexpr std::string_view kManifest = R"json({
    "id": "org.example.view",
    "name": "View",
    "version": "0.1.0",
    "api_version": 1,
    "min_platform_version": "0.4.0",
    "kind": "view",
    "requires": [],
    "contributes": [
      { "slot": "page.side-panel", "id": "panel", "title": "Panel", "surface": "native" },
      { "slot": "search.results", "id": "hits", "title": "Hits", "surface": "declarative" }
    ],
    "permissions": {}
  })json";

  struct ViewState {
    int created = 0;
    std::string contribution;
    IPluginContext* context = nullptr;
  };
  auto state = std::make_shared<ViewState>();
  class ViewPlugin final : public IViewPlugin {
   public:
    explicit ViewPlugin(std::shared_ptr<ViewState> state) : state_(std::move(state)) {}
    auto Start(IPluginContext& context) -> bool override {
      state_->context = &context;
      return true;
    }
    void Stop() override {
      state_->context = nullptr;
    }
    auto CreateView(IViewContext& context) -> QWidget* override {
      ++state_->created;
      state_->contribution = std::string(context.ContributionId());
      return reinterpret_cast<QWidget*>(static_cast<std::uintptr_t>(1));
    }

   private:
    std::shared_ptr<ViewState> state_;
  };

  Require(host.Register(kManifest, PluginTier::kNative,
                        [state]() -> std::unique_ptr<IPlugin> {
                          return std::make_unique<ViewPlugin>(state);
                        })
              .empty(),
          "view plugin registers");
  host.LoadAll();
  host.StartAll();
  Require(host.State("org.example.view") == PluginState::kStarted, "view plugin started");

  FakeViewContext context;
  context.plugin_id = "org.example.view";
  context.contribution_id = "panel";
  Require(host.Ui().Activate("org.example.view", "panel", context), "native view created");
  Require(state->created == 1, "factory invoked once");
  Require(state->contribution == "panel", "factory receives contribution id");

  SurfaceNode node;
  node.type = "panel";
  SurfaceNode child;
  child.type = "text";
  child.text = "live";
  node.children.push_back(std::move(child));
  Require(state->context != nullptr, "plugin context alive");
  Require(state->context->Ui().UpdateSurface("hits", node), "plugin publishes surface");
  Require(host.Ui().Surface("org.example.view", "hits") != nullptr, "surface stored");

  std::string action;
  Require(state->context->Ui().OnSurfaceEvent(
              "hits", [&action](const SurfaceEvent& event) { action = event.action_id; }),
          "plugin registers surface sink");
  Require(host.Ui().Dispatch(
              "org.example.view",
              SurfaceEvent{.surface_id = "hits", .action_id = "open", .values = {{"id", "42"}}}),
          "surface event dispatched");
  Require(action == "open", "surface event reached plugin");
  Require(!state->context->Ui().UpdateSurface("missing", node), "unknown contribution rejected");

  host.StopAll(std::chrono::seconds(2));
}

auto TestHostStopTimeout() -> void {
  ThreadExecutor executor;
  {
    PluginHost host(executor, PlatformServices{});
    PluginBehavior slow;
    slow.stop_sleep = [] { std::this_thread::sleep_for(std::chrono::milliseconds(250)); };
    Require(host.Register(MakeServiceManifest("org.example.slow"), PluginTier::kNative,
                          MakeFactory(slow))
                .empty(),
            "slow plugin registers");
    host.LoadAll();
    host.StartAll();
    Require(host.State("org.example.slow") == PluginState::kStarted, "slow plugin started");

    const auto begin = std::chrono::steady_clock::now();
    host.StopAll(std::chrono::milliseconds(50));
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - begin)
                             .count();
    Require(elapsed < 200, "stop deadline enforced");
    executor.Stop();
    Require(host.State("org.example.slow") == PluginState::kStopped,
            "slow plugin eventually stopped");
  }
}

}  // namespace

auto main() -> int {
  TestManifestParsing();
  TestTopics();
  TestSurfaceParsing();
  TestEventBus();
  TestUiRegistry();
  TestHostLifecycle();
  TestHostFailures();
  TestHostDisplay();
  TestHostStopTimeout();
  spdlog::info("cppwiki_plugins_tests passed");
  return 0;
}
