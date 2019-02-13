#pragma once

#include <libyaaf/exports.h>
#include <libyaaf/utils/async/locker.h>
#include <libyaaf/utils/async/task.h>
#include <libyaaf/utils/utils.h>

#include <algorithm>
#include <deque>
#include <shared_mutex>

namespace yaaf {
namespace utils {
namespace async {

using task_queue_t = std::deque<task_wrapper_ptr>;

class threads_pool : public utils::non_copy {
public:
  struct params_t {
    size_t threads_count;
    thread_kind_t kind;
    params_t(size_t _threads_count, thread_kind_t _kind) {
      threads_count = _threads_count;
      kind = _kind;
    }
  };
  EXPORT threads_pool(const params_t &p);
  EXPORT ~threads_pool();
  EXPORT task_result_ptr post(const task_wrapper_ptr &task);
  EXPORT void flush();
  EXPORT void stop();

  size_t threads_count() const { return _params.threads_count; }
  thread_kind_t kind() const { return _params.kind; }
  bool is_stopped() const { return _is_stoped; }

  size_t active_workers() const {
    std::shared_lock<std::shared_mutex> lg(_queue_mutex);
    size_t res = _in_queue.size();
    return res + (_task_runned);
  }

protected:
  void _pool_logic(size_t num);
  void push_task(const task_wrapper_ptr &at);

protected:
  params_t _params;
  std::vector<std::thread> _threads;
  task_queue_t _in_queue;
  mutable std::shared_mutex _queue_mutex;
  std::condition_variable_any _condition;
  std::atomic_bool _stop_flag;         // true - pool under stop.
  bool _is_stoped;                 // true - already stopped.
  std::atomic_size_t _task_runned; // count of runned tasks.
};
} // namespace async
} // namespace utils
} // namespace yaaf
