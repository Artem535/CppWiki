#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

#include "plugins/display_registry.h"
#include "plugins/manifest.h"
#include "plugins/surface.h"
#include "plugins/surface_renderer.h"

namespace {

using namespace cppwiki::plugins;

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

auto FindButton(QWidget& root, const QString& text) -> QPushButton* {
  for (auto* button : root.findChildren<QPushButton*>()) {
    if (button->text() == text) {
      return button;
    }
  }
  return nullptr;
}

constexpr std::string_view kSurface = R"json(
{
  "surface_version": 1,
  "type": "panel",
  "children": [
    { "type": "text", "text": "Outline sync" },
    { "type": "section", "title": "Status", "children": [
      { "type": "key-value", "items": [["Last sync", "2 minutes ago"]] },
      { "type": "progress", "value": 0.4, "label": "Indexing" }
    ]},
    { "type": "table", "columns": ["Page", "State"], "rows": [["A", "ok"], ["B", "stale"]] },
    { "type": "list", "items": ["one", "two"] },
    { "type": "form", "fields": [
      { "name": "query", "label": "Query", "type": "text", "value": "initial" },
      { "name": "count", "label": "Count", "type": "number", "value": "5" },
      { "name": "enabled", "label": "Enabled", "type": "boolean", "value": "true" },
      { "name": "mode", "label": "Mode", "type": "select", "value": "fast",
        "options": ["fast", "full"] }
    ]},
    { "type": "actions", "items": [{ "id": "sync", "label": "Sync now", "style": "primary" }] },
    { "type": "badge", "text": "beta" },
    { "type": "markdown", "text": "**bold** text" }
  ]
}
)json";

auto TestRendering() -> void {
  auto parsed = ParseSurface(kSurface);
  Require(parsed.root.has_value(), "surface parses: " + parsed.error);

  SurfaceEvent captured;
  bool called = false;
  SurfaceRenderContext context;
  context.surface_id = "sync";
  context.on_event = [&captured, &called](const SurfaceEvent& event) {
    captured = event;
    called = true;
  };

  QtSurfaceRenderer renderer;
  std::unique_ptr<QWidget> widget(renderer.Render(*parsed.root, context));
  Require(widget != nullptr, "renderer returns a widget");

  bool text_found = false;
  for (auto* label : widget->findChildren<QLabel*>()) {
    if (label->text() == "Outline sync") {
      text_found = true;
    }
  }
  Require(text_found, "text primitive rendered");

  auto* table = widget->findChild<QTableWidget*>();
  Require(table != nullptr, "table rendered");
  Require(table->rowCount() == 2 && table->columnCount() == 2, "table dimensions");
  Require(table->item(1, 1) != nullptr && table->item(1, 1)->text() == "stale", "table cell text");

  auto* progress = widget->findChild<QProgressBar*>();
  Require(progress != nullptr, "progress rendered");
  Require(progress->value() == 40, "progress value");

  auto* list = widget->findChild<QListWidget*>();
  Require(list != nullptr && list->count() == 2, "list rendered");

  auto* query = widget->findChild<QLineEdit*>("surface-field-query");
  auto* count = widget->findChild<QDoubleSpinBox*>("surface-field-count");
  auto* enabled = widget->findChild<QCheckBox*>("surface-field-enabled");
  auto* mode = widget->findChild<QComboBox*>("surface-field-mode");
  Require(query != nullptr, "text field rendered");
  Require(count != nullptr, "number field rendered");
  Require(enabled != nullptr, "boolean field rendered");
  Require(mode != nullptr, "select field rendered");
  Require(count->value() == 5.0, "number field value");
  Require(enabled->isChecked(), "boolean field value");
  Require(mode->currentText() == "fast", "select field value");

  query->setText("changed");
  mode->setCurrentText("full");

  auto* button = FindButton(*widget, "Sync now");
  Require(button != nullptr, "action button rendered");
  button->click();
  Require(called, "action click delivered");
  Require(captured.surface_id == "sync", "surface id");
  Require(captured.action_id == "sync", "action id");
  Require(captured.values.at("query") == "changed", "text value collected");
  Require(captured.values.at("count") == "5", "number value collected");
  Require(captured.values.at("enabled") == "true", "boolean value collected");
  Require(captured.values.at("mode") == "full", "select value collected");
}

auto TestNativeViewActivation() -> void {
  auto parsed = ParseManifest(R"json({
    "id": "org.example.view",
    "name": "View",
    "version": "0.1.0",
    "api_version": 1,
    "min_platform_version": "0.4.0",
    "kind": "view",
    "contributes": [
      { "slot": "page.side-panel", "id": "panel", "title": "Panel", "surface": "native" }
    ]
  })json");
  Require(parsed.manifest.has_value(), "manifest parses");
  UiRegistry registry;
  Require(registry
              .Register(*parsed.manifest, PluginTier::kNative,
                        [](IViewContext& context) -> QWidget* {
                          (void)context;
                          return new QWidget();
                        })
              .empty(),
          "native contribution registers");

  class FakeContext final : public IViewContext {
   public:
    [[nodiscard]] auto PluginId() const -> std::string_view override {
      return "org.example.view";
    }
    [[nodiscard]] auto ContributionId() const -> std::string_view override {
      return "panel";
    }
    [[nodiscard]] auto ThemeToken(std::string_view) const -> std::string override {
      return {};
    }
    auto RunCommand(std::string_view, std::string_view) -> bool override {
      return false;
    }
    auto SetSurface(const SurfaceNode&) -> bool override {
      return false;
    }
  } context;

  Require(registry.Activate("org.example.view", "panel", context), "native view activates");
  const auto* record = registry.Find("org.example.view", "panel");
  Require(record != nullptr && record->view != nullptr, "native view stored");
  auto* view = record->view;
  registry.Deactivate("org.example.view", "panel");
  Require(!registry.IsActive("org.example.view", "panel"), "native view deactivated");
  delete view;
}

}  // namespace

auto main(int argc, char** argv) -> int {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  (void)app;
  TestRendering();
  TestNativeViewActivation();
  std::cout << "cppwiki_plugins_ui_tests passed\n";
  return 0;
}
