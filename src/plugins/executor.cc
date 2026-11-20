#include "plugins/executor.h"

#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <utility>

namespace cppwiki::plugins {

void ManualExecutor::Post(std::function<void()> task) {
  if (task) {
    tasks_.push_back(std::move(task));
  }
}

auto ManualExecutor::Pending() const -> std::size_t {
  return tasks_.size();
}

void ManualExecutor::RunAll() {
  auto tasks = std::move(tasks_);
  tasks_.clear();
  for (auto& task : tasks) {
    task();
  }
}

void ManualExecutor::Clear() {
  tasks_.clear();
}

ThreadExecutor::ThreadExecutor() : thread_([this] { Run(); }) {}

ThreadExecutor::~ThreadExecutor() {
  Stop();
}

void ThreadExecutor::Post(std::function<void()> task) {
  if (!task) {
    return;
  }
  {
    const std::lock_guard lock(mutex_);
    if (stopping_ || stopped_) {
      return;
    }
    tasks_.push_back(std::move(task));
  }
  wakeup_.notify_one();
}

auto ThreadExecutor::IsExecutorThread() const -> bool {
  return thread_.get_id() == std::this_thread::get_id();
}

void ThreadExecutor::Stop() {
  {
    const std::lock_guard lock(mutex_);
    if (stopping_ || stopped_) {
      if (thread_.joinable() && !IsExecutorThread()) {
        thread_.join();
      }
      return;
    }
    stopping_ = true;
  }
  wakeup_.notify_all();
  if (thread_.joinable() && !IsExecutorThread()) {
    thread_.join();
  }
}

void ThreadExecutor::Run() {
  for (;;) {
    std::function<void()> task;
    {
      std::unique_lock lock(mutex_);
      wakeup_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
      if (tasks_.empty()) {
        if (stopping_) {
          stopped_ = true;
          return;
        }
        continue;
      }
      task = std::move(tasks_.front());
      tasks_.pop_front();
    }
    task();
  }
}

}  // namespace cppwiki::plugins
