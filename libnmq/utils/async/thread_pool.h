#pragma once

#include <libnmq/exports.h>
#include <libnmq/utils/async/locker.h>
#include <libnmq/utils/utils.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace nmq {
namespace utils {
namespace async {

using thread_kind_t = uint16_t;

enum class THREAD_KINDS : thread_kind_t { DISK_IO = 1, COMMON };

enum class TASK_PRIORITY : uint8_t {
  DEFAULT = 0,
  WORKER = std::numeric_limits<uint8_t>::max()
};

#ifdef DEBUG
#define TKIND_CHECK(expected, exists)                                                    \
  if ((thread_kind_t)expected != exists) {                                               \
    throw MAKE_EXCEPTION("wrong thread kind");                                           \
  }
#else //  DEBUG
#define TKIND_CHECK(expected, exists)                                                    \
  (void)(expected);                                                                      \
  (void)(exists);
#endif

struct thread_info {
  thread_kind_t kind;
  size_t thread_number;
};

struct task_result {
  bool runned;
  locker
      m; // dont use mutex. mutex::lock() requires that the calling thread owns the mutex.
  task_result() {
    runned = true;
    m.lock();
  }
  ~task_result() {}
  void wait() {
    m.lock();
    m.unlock();
  }

  void unlock() {
    runned = false;
    m.unlock();
  }
};

using task_result_ptr = std::shared_ptr<task_result>;

enum class RUN_STRATEGY { SINGLE, REPEAT };

using async_task = std::function<RUN_STRATEGY(const thread_info &)>;

class async_task_wrapper {
public:
  EXPORT async_task_wrapper(async_task &t, const std::string &_function,
                            const std::string &file, int line);
  EXPORT async_task_wrapper(async_task &t, const std::string &_function,
                            const std::string &file, int line, TASK_PRIORITY p);
  EXPORT RUN_STRATEGY apply(const thread_info &ti);
  EXPORT task_result_ptr result() const;

  TASK_PRIORITY priority;

private:
  /// return true if need recall.
  RUN_STRATEGY worker();

private:
  thread_info _tinfo;
  task_result_ptr _result;
  async_task _task;
  std::string _parent_function;
  std::string _code_file;
  int _code_line;
};

using async_task_wrapper_ptr = std::shared_ptr<async_task_wrapper>;

#define AT(task)                                                                         \
  std::make_shared<async_task_wrapper>(task, std::string(__FUNCTION__),                  \
                                       std::string(__FILE__), __LINE__)

#define AT_PRIORITY(task, pr)                                                            \
  std::make_shared<async_task_wrapper>(task, std::string(__FUNCTION__),                  \
                                       std::string(__FILE__), __LINE__, pr)

using task_queue_t = std::deque<async_task_wrapper_ptr>;

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
  size_t threads_count() const { return _params.threads_count; }
  thread_kind_t kind() const { return _params.kind; }

  bool is_stopped() const { return _is_stoped; }

  EXPORT task_result_ptr post(const async_task_wrapper_ptr &task);
  EXPORT void flush();
  EXPORT void stop();

  size_t active_works() const {
    std::shared_lock<std::shared_mutex> lg(_queue_mutex);
    size_t res = _in_queue.size();
    return res + (_task_runned);
  }

protected:
  void _pool_logic(size_t num);
  void push_task(const async_task_wrapper_ptr &at);

protected:
  params_t _params;
  std::vector<std::thread> _threads;
  task_queue_t _in_queue;
  mutable std::shared_mutex _queue_mutex;
  std::condition_variable_any _condition;
  bool _stop_flag;                 // true - pool under stop.
  bool _is_stoped;                 // true - already stopped.
  std::atomic_size_t _task_runned; // count of runned tasks.
};
} // namespace async
} // namespace utils
} // namespace nmq
