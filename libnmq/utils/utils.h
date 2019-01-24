#pragma once

#include <libnmq/utils/exception.h>
#include <atomic>

#define NOT_IMPLEMENTED THROW_EXCEPTION("Not implemented");

#ifdef DOUBLE_CHECKS
#define ENSURE_MSG(A, E)                                                                 \
  if (!(A)) {                                                                            \
    THROW_EXCEPTION(E);                                                                  \
  }
#define ENSURE(A) ENSURE_MSG(A, #A)
#else
#define ENSURE_MSG(A, E)
#define ENSURE(A)
#endif

#define UNUSED(x) (void)(x)

namespace nmq {
namespace utils {

inline void sleep_mls(long long a) {
  std::this_thread::sleep_for(std::chrono::milliseconds(a));
}

class non_copy {
private:
  non_copy(const non_copy &) = delete;
  non_copy &operator=(const non_copy &) = delete;

protected:
  non_copy() = default;
};

struct elapsed_time {
  elapsed_time() { start_time = clock(); }

  double elapsed() { return double(clock() - start_time) / CLOCKS_PER_SEC; }
  clock_t start_time;
};

struct Waitable {
  Waitable() {
    _start_begin.store(false);
    _stop_begin.store(true);
    _started.store(false);
    _stoped.store(true);
  }

  ~Waitable() {
    if (!isStoped()) {
      logger_fatal("Process was not stopped correctly");
      std::abort();
    }
  }
  bool isStartBegin() const { return _start_begin.load(); }
  bool isStarted() const { return _started.load(); }
  bool isStopBegin() const { return _stop_begin.load(); }
  bool isStoped() const { return _stoped.load(); }

  void startBegin() {
    if (_start_begin.load()) {
      throw std::logic_error("Double start");
    }
    _stop_begin.store(false);
    _start_begin.store(true);
  }

  void startComplete() {
    _started.store(true);
    _stoped.store(false);
  }

  void stopBegin() {
    _start_begin.store(false);
    _stop_begin.store(true);
  }

  void stopComplete() {
    _started.store(false);
    _stoped.store(true);
  }

  void waitStarting() {
    ENSURE(_start_begin);
    while (!_started.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  void waitStoping() {
    ENSURE(_stop_begin);
    while (!_stoped.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  std::atomic_bool _start_begin;
  std::atomic_bool _stop_begin;

  std::atomic_bool _started;
  std::atomic_bool _stoped;
};
} // namespace utils
} // namespace nmq
