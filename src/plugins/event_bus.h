#ifndef CPPWIKI_SRC_PLUGINS_EVENT_BUS_H_
#define CPPWIKI_SRC_PLUGINS_EVENT_BUS_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "plugins/api/services.h"
#include "plugins/executor.h"

namespace cppwiki::plugins {

struct SubscriptionOptions {
  std::size_t max_queue = 256;
  bool drop_newest = false;
};

// In-process event bus for versioned topics (`<domain>.<subject>.vN`). Delivery is
// asynchronous on the platform executor, at-most-once, ordered per publisher and topic.
class EventBus final {
 public:
  explicit EventBus(IExecutor& executor);
  ~EventBus();

  EventBus(const EventBus&) = delete;
  EventBus& operator=(const EventBus&) = delete;

  // Returns 0 for an invalid topic or an empty handler.
  auto Subscribe(std::string_view topic, EventHandler handler, SubscriptionOptions options = {})
      -> std::uint64_t;
  void Unsubscribe(std::uint64_t subscription_id);

  // Returns the number of subscriptions the event was queued for.
  auto Publish(std::string_view topic, std::string_view payload_json) -> std::size_t;

  [[nodiscard]] auto DroppedCount() const -> std::uint64_t;
  [[nodiscard]] auto InvalidTopicCount() const -> std::uint64_t;
  [[nodiscard]] auto IsValidTopic(std::string_view topic) const -> bool;

 private:
  struct State;
  std::shared_ptr<State> state_;
};

}  // namespace cppwiki::plugins

#endif  // CPPWIKI_SRC_PLUGINS_EVENT_BUS_H_
