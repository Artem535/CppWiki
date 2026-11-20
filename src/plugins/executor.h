#ifndef CPPWIKI_SRC_PLUGINS_EXECUTOR_H_
#define CPPWIKI_SRC_PLUGINS_EXECUTOR_H_

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace cppwiki::plugins {

// Platform-owned task executor. Host services (event delivery, plugin start/stop) run here,
// never on the UI thread.
class IExecutor {
 public:
  virtual ~IExecutor() = default;

  virtual void Post(std::function<void()> task) = 0;

  // True when called from the executor's own thread. Stop paths use this to avoid posting a
  // task and waiting for it from the thread that would run it.
  [[nodiscard]] virtual auto IsExecutorThread() const -> bool = 0;
};

// Runs tasks synchronously. For tests and for deterministic host behaviour.
class InlineExecutor final : public IExecutor {
 public:
  void Post(std::function<void()> task) override {
    if (task) {
      task();
    }
  }
  [[nodiscard]] auto IsExecutorThread() const -> bool override {
    return true;
  }
};

// Queues tasks and runs them only when RunAll() is called. Tests use it to observe async
// delivery and backpressure without threads.
class ManualExecutor final : public IExecutor {
 public:
  void Post(std::function<void()> task) override;
  [[nodiscard]] auto IsExecutorThread() const -> bool override {
    return true;
  }
  [[nodiscard]] auto Pending() const -> std::size_t;
  void RunAll();
  void Clear();

 private:
  std::vector<std::function<void()>> tasks_;
};

// Single background thread. Stop() drains queued tasks and joins.
class ThreadExecutor final : public IExecutor {
 public:
  ThreadExecutor();
  ~ThreadExecutor() override;

  ThreadExecutor(const ThreadExecutor&) = delete;
  ThreadExecutor& operator=(const ThreadExecutor&) = delete;

  void Post(std::function<void()> task) override;
  [[nodiscard]] auto IsExecutorThread() const -> bool override;
  void Stop();

 private:
  void Run();

  std::mutex mutex_;
  std::condition_variable wakeup_;
  std::deque<std::function<void()>> tasks_;
  std::thread thread_;
  bool stopping_ = false;
  bool stopped_ = false;
};

}  // namespace cppwiki::plugins

#endif  // CPPWIKI_SRC_PLUGINS_EXECUTOR_H_
