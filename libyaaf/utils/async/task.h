#pragma once

#include <libyaaf/exports.h>
#include <libyaaf/utils/async/locker.h>
#include <libyaaf/utils/utils.h>

namespace yaaf {
namespace utils {
namespace async {

using thread_kind_t = uint16_t;

enum class TASK_PRIORITY : uint8_t {
  DEFAULT = 0,
  WORKER = std::numeric_limits<uint8_t>::max()
};

#ifdef DOUBLE_CHECKS
#define TKIND_CHECK(expected, exists)                                                    \
  if ((thread_kind_t)expected != exists) {                                               \
    throw MAKE_EXCEPTION("wrong thread kind");                                           \
  }
#else //  DOUBLE_CHECKS
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
  task_result() noexcept {
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

enum class CONTINUATION_STRATEGY { SINGLE, REPEAT };

using task = std::function<CONTINUATION_STRATEGY(const thread_info &)>;

class task_wrapper {
public:
  EXPORT task_wrapper(task &t, const std::string &_function, const std::string &file,
                      int line);
  EXPORT task_wrapper(task &t, const std::string &_function, const std::string &file,
                      int line, TASK_PRIORITY p);
  EXPORT CONTINUATION_STRATEGY apply(const thread_info &ti);
  EXPORT task_result_ptr result() const;

  TASK_PRIORITY priority;

private:
  /// return true if need recall.
  CONTINUATION_STRATEGY worker();

private:
  thread_info _tinfo;
  task_result_ptr _result;
  task _task;
  std::string _parent_function;
  std::string _code_file;
  int _code_line;
};

using task_wrapper_ptr = std::shared_ptr<task_wrapper>;

#define wrap_task(t)                                                                     \
  std::make_shared<task_wrapper>(t, std::string(__FUNCTION__), std::string(__FILE__),    \
                                 __LINE__)

#define wrap_task_with_priority(t, pr)                                                   \
  std::make_shared<task_wrapper>(t, std::string(__FUNCTION__), std::string(__FILE__),    \
                                 __LINE__, pr)
} // namespace async
} // namespace utils
} // namespace yaaf
