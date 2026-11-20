#include "plugins/display_registry.h"

#include <algorithm>
#include <string>
#include <utility>

namespace cppwiki::plugins {

namespace {

auto KeyOf(std::string_view plugin_id, std::string_view contribution_id)
    -> std::pair<std::string, std::string> {
  return {std::string(plugin_id), std::string(contribution_id)};
}

}  // namespace

auto PluginSlots() -> const std::vector<SlotDefinition>& {
  static const std::vector<SlotDefinition> kSlots = {
      {"rail.mode", true, true, true, false},      {"page.side-panel", true, true, true, false},
      {"page.full-tab", true, true, true, false},  {"document.editor", true, false, false, true},
      {"search.results", true, true, true, false}, {"settings.section", true, true, true, false},
      {"status.item", false, true, true, false},
  };
  return kSlots;
}

auto FindPluginSlot(std::string_view id) -> const SlotDefinition* {
  const auto& slots
  = PluginSlots();
  const auto it = std::find_if(slots.begin(), slots.end(),
                               [id](const SlotDefinition& slot) { return slot.id == id; });
  return it == slots
  .end() ? nullptr : &*it;
}

auto UiRegistry::Validate(const PluginManifest& manifest, PluginTier tier,
                          bool has_native_factory) const -> std::string {
  // Validate everything first so a rejected plugin never leaves half its contributions behind.
  for (const auto& contribution : manifest.contributes) {
    const auto* slot = FindPluginSlot(contribution.slot);
    if (slot == nullptr) {
      return "unknown slot '" + contribution.slot + "'";
    }
    if (contribution.surface == SurfaceKind::kNative) {
      if (!slot->allow_native) {
        return "slot '" + contribution.slot + "' does not accept native views";
      }
      if (tier != PluginTier::kNative) {
        return "native contribution '" + contribution.id + "' requires a native (Tier A) plugin";
      }
      if (!has_native_factory) {
        return "native contribution '" + contribution.id + "' has no view factory";
      }
    } else if (!slot->allow_declarative) {
      return "slot '" + contribution.slot + "' does not accept declarative surfaces";
    }
    if (slot->requires_document_kind && contribution.document_kind.empty()) {
      return "contribution '" + contribution.id + "' must declare 'document_kind'";
    }
    if (contribution.slot == "document.editor") {
      for (const auto* record : Contributions("document.editor")) {
        if (record->contribution.document_kind == contribution.document_kind) {
          return "document kind '" + contribution.document_kind +
                 "' already has an editor contribution";
        }
      }
    }
    if (!slot->multiple) {
      const auto existing = Contributions(contribution.slot);
      if (!existing.empty()) {
        return "slot '" + contribution.slot + "' accepts a single contribution";
      }
    }
    if (Find(manifest.id, contribution.id) != nullptr) {
      return "duplicate contribution '" + contribution.id + "'";
    }
  }

  return {};
}

auto UiRegistry::Register(const PluginManifest& manifest, PluginTier tier,
                          std::function<QWidget*(IViewContext&)> native_factory) -> std::string {
  const auto error = Validate(manifest, tier, static_cast<bool>(native_factory));
  if (!error.empty()) {
    return error;
  }
  for (const auto& contribution : manifest.contributes) {
    ContributionRecord record;
    record.contribution = contribution;
    record.plugin_id = manifest.id;
    record.tier = tier;
    records_.push_back(std::move(record));
    if (native_factory) {
      factories_[KeyOf(manifest.id, contribution.id)] = native_factory;
    }
  }
  return {};
}

auto UiRegistry::Contributions(std::string_view slot) const
    -> std::vector<const ContributionRecord*> {
  std::vector<const ContributionRecord*> result;
  for (const auto& record : records_) {
    if (record.contribution.slot == slot) {
      result.push_back(&record);
    }
  }
  return result;
}

auto UiRegistry::AllContributions() const -> const std::vector<ContributionRecord>& {
  return records_;
}

auto UiRegistry::FindMutable(std::string_view plugin_id, std::string_view contribution_id)
    -> ContributionRecord* {
  const auto it = std::find_if(records_.begin(), records_.end(),
                               [plugin_id, contribution_id](const ContributionRecord& record) {
                                 return record.plugin_id == plugin_id &&
                                        record.contribution.id == contribution_id;
                               });
  return it == records_.end() ? nullptr : &*it;
}

auto UiRegistry::Find(std::string_view plugin_id, std::string_view contribution_id) const
    -> const ContributionRecord* {
  const auto it = std::find_if(records_.begin(), records_.end(),
                               [plugin_id, contribution_id](const ContributionRecord& record) {
                                 return record.plugin_id == plugin_id &&
                                        record.contribution.id == contribution_id;
                               });
  return it == records_.end() ? nullptr : &*it;
}

auto UiRegistry::Activate(std::string_view plugin_id, std::string_view contribution_id,
                          IViewContext& context) -> bool {
  auto* record = FindMutable(plugin_id, contribution_id);
  if (record == nullptr) {
    return false;
  }
  if (record->contribution.surface != SurfaceKind::kNative) {
    return false;
  }
  if (record->active) {
    return true;
  }
  const auto factory = factories_.find(KeyOf(plugin_id, contribution_id));
  if (factory == factories_.end()) {
    return false;
  }
  auto* view = factory->second(context);
  if (view == nullptr) {
    return false;
  }
  record->view = view;
  record->active = true;
  return true;
}

void UiRegistry::Deactivate(std::string_view plugin_id, std::string_view contribution_id) {
  auto* record = FindMutable(plugin_id, contribution_id);
  if (record == nullptr) {
    return;
  }
  record->active = false;
  record->view = nullptr;
}

auto UiRegistry::IsActive(std::string_view plugin_id, std::string_view contribution_id) const
    -> bool {
  const auto* record = Find(plugin_id, contribution_id);
  return record != nullptr && record->active;
}

auto UiRegistry::UpdateSurface(std::string_view plugin_id, std::string_view contribution_id,
                               const SurfaceNode& surface) -> bool {
  const auto* record = Find(plugin_id, contribution_id);
  if (record == nullptr) {
    return false;
  }
  if (!ValidateSurface(surface).empty()) {
    return false;
  }
  surfaces_[KeyOf(plugin_id, contribution_id)] = surface;
  if (surface_observer_) {
    surface_observer_(std::string(plugin_id), std::string(contribution_id), surface);
  }
  return true;
}

auto UiRegistry::Surface(std::string_view plugin_id, std::string_view contribution_id) const
    -> const SurfaceNode* {
  const auto it = surfaces_.find(KeyOf(plugin_id, contribution_id));
  return it == surfaces_.end() ? nullptr : &it->second;
}

auto UiRegistry::SetSurfaceEventSink(std::string_view plugin_id, std::string_view contribution_id,
                                     std::function<void(const SurfaceEvent&)> sink) -> bool {
  if (Find(plugin_id, contribution_id) == nullptr || !sink) {
    return false;
  }
  event_sinks_[KeyOf(plugin_id, contribution_id)] = std::move(sink);
  return true;
}

auto UiRegistry::Dispatch(const std::string& plugin_id, const SurfaceEvent& event) -> bool {
  const auto it = event_sinks_.find(KeyOf(plugin_id, event.surface_id));
  if (it == event_sinks_.end()) {
    return false;
  }
  it->second(event);
  return true;
}

void UiRegistry::SetSurfaceObserver(
    std::function<void(const std::string&, const std::string&, const SurfaceNode&)> observer) {
  surface_observer_ = std::move(observer);
}

void UiRegistry::RemovePlugin(std::string_view plugin_id) {
  records_.erase(std::remove_if(records_.begin(), records_.end(),
                                [plugin_id](const ContributionRecord& record) {
                                  return record.plugin_id == plugin_id;
                                }),
                 records_.end());
  const auto erase_for = [plugin_id](auto& map) {
    for (auto it = map.begin(); it != map.end();) {
      if (it->first.first == plugin_id) {
        it = map.erase(it);
      } else {
        ++it;
      }
    }
  };
  erase_for(factories_);
  erase_for(surfaces_);
  erase_for(event_sinks_);
}

void UiRegistry::Clear() {
  records_.clear();
  factories_.clear();
  surfaces_.clear();
  event_sinks_.clear();
  surface_observer_ = {};
}

}  // namespace cppwiki::plugins
