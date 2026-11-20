#include "plugins/event_bus.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "plugins/manifest.h"

namespace cppwiki::plugins {

struct EventBus::State {
  struct Subscription {
    std::uint64_t id = 0;
    std::string topic;
    EventHandler handler;
    SubscriptionOptions options;
    std::deque<std::string> queue;
    bool scheduled = false;
    bool active = true;
  };

  explicit State(IExecutor& task_executor) : executor(&task_executor) {}

  IExecutor* executor;
  mutable std::mutex mutex;
  std::map<std::uint64_t, Subscription> subscriptions;
  std::uint64_t next_id = 1;

  std::atomic<std::uint64_t> dropped{0};
  std::atomic<std::uint64_t> invalid_topics{0};

  // Must be called with `mutex` held. Returns true when the caller still has to post Drain()
  // after unlocking; posting inline under the lock would deadlock with an InlineExecutor.
  auto MarkScheduled(std::uint64_t id) -> bool {
    const auto it = subscriptions.find(id);
    if (it == subscriptions.end() || it->second.scheduled) {
      return false;
    }
    it->second.scheduled = true;
    return true;
  }

  void Drain(std::uint64_t id) {
    for (;;) {
      std::string payload;
      EventHandler handler;
      {
        const std::lock_guard lock(mutex);
        const auto it = subscriptions.find(id);
        if (it == subscriptions.end() || !it->second.active) {
          if (it != subscriptions.end()) {
            it->second.scheduled = false;
          }
          return;
        }
        if (it->second.queue.empty()) {
          it->second.scheduled = false;
          return;
        }
        payload = std::move(it->second.queue.front());
        it->second.queue.pop_front();
        handler = it->second.handler;
      }
      try {
        handler(payload);
      } catch (...) {
        // A plugin handler must not take the bus down. The failure is swallowed here; the
        // platform log records plugin failures at the host boundary.
      }
    }
  }
};

EventBus::EventBus(IExecutor& executor) : state_(std::make_shared<State>(executor)) {}

EventBus::~EventBus() {
  const std::lock_guard lock(state_->mutex);
  for (auto& [id, subscription] : state_->subscriptions) {
    (void)id;
    subscription.active = false;
    subscription.handler = {};
    subscription.queue.clear();
  }
}

auto EventBus::IsValidTopic(std::string_view topic) const -> bool {
  return ValidateTopic(topic);
}

auto EventBus::Subscribe(std::string_view topic, EventHandler handler, SubscriptionOptions options)
    -> std::uint64_t {
  if (!ValidateTopic(topic) || !handler) {
    return 0;
  }
  if (options.max_queue == 0) {
    options.max_queue = 1;
  }
  const std::lock_guard lock(state_->mutex);
  const auto id = state_->next_id++;
  State::Subscription subscription;
  subscription.id = id;
  subscription.topic = std::string(topic);
  subscription.handler = std::move(handler);
  subscription.options = options;
  state_->subscriptions.emplace(id, std::move(subscription));
  return id;
}

void EventBus::Unsubscribe(std::uint64_t subscription_id) {
  const std::lock_guard lock(state_->mutex);
  const auto it = state_->subscriptions.find(subscription_id);
  if (it == state_->subscriptions.end()) {
    return;
  }
  it->second.active = false;
  it->second.handler = {};
  it->second.queue.clear();
}

auto EventBus::Publish(std::string_view topic, std::string_view payload_json) -> std::size_t {
  if (!ValidateTopic(topic)) {
    state_->invalid_topics.fetch_add(1, std::memory_order_relaxed);
    return 0;
  }
  std::size_t queued = 0;
  std::vector<std::uint64_t> to_schedule;
  {
    const std::lock_guard lock(state_->mutex);
    for (auto& [id, subscription] : state_->subscriptions) {
      if (!subscription.active || subscription.topic != topic) {
        continue;
      }
      if (subscription.queue.size() >= subscription.options.max_queue) {
        if (subscription.options.drop_newest) {
          state_->dropped.fetch_add(1, std::memory_order_relaxed);
          continue;
        }
        subscription.queue.pop_front();
        state_->dropped.fetch_add(1, std::memory_order_relaxed);
      }
      subscription.queue.emplace_back(payload_json);
      ++queued;
      if (state_->MarkScheduled(id)) {
        to_schedule.push_back(id);
      }
    }
  }
  const auto self = state_;
  for (const auto id : to_schedule) {
    self->executor->Post([self, id] { self->Drain(id); });
  }
  return queued;
}

auto EventBus::DroppedCount() const -> std::uint64_t {
  return state_->dropped.load(std::memory_order_relaxed);
}

auto EventBus::InvalidTopicCount() const -> std::uint64_t {
  return state_->invalid_topics.load(std::memory_order_relaxed);
}

}  // namespace cppwiki::plugins
