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

class long_process {
public:
  long_process() = delete;

  long_process(const std::string &name, bool checkOnDtor_)
      : _name(name), checkOnDtor(checkOnDtor_) {
    _started.store(false);
    _complete.store(false);
  }

  ~long_process() {
    if (checkOnDtor && !is_complete()) {
      logging::logger_fatal(_name + " Process was not _completed correctly");
      std::abort();
    }
  }

  bool is_started() const { return _started.load(); }
  bool is_complete() const { return _complete.load(); }

  void start(bool checkDoubleStarting = true) {
    if (checkDoubleStarting && _started.load()) {
      throw std::logic_error(_name + " Double start");
    }

    _started.store(true);
    _complete.store(false);
  }

  void complete(bool checkTwiceStoping = false) {
    if (!_started.load()) {
      throw std::logic_error(_name + " _started is false");
    }
    if (checkTwiceStoping && _complete.load()) {
      throw std::logic_error(_name + " Double complete");
    }
    _complete.store(true);
  }

  void reset() {
    _started.store(false);
    _complete.store(false);
  }

  void wait() {
    ENSURE(_started);
    while (!_complete.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

private:
  std::string _name;
  bool checkOnDtor;
  std::atomic_bool _started;
  std::atomic_bool _complete;
};

struct waitable {
  waitable() : starting("starting", false), stoping("stoping", true) {}

  ~waitable() {
    if (!is_stoped()) {
      logging::logger_fatal("Process was not stopped correctly");
      std::abort();
    }
  }
  bool is_start_begin() const { return starting.is_started(); }
  bool is_started() const { return starting.is_complete(); }
  bool is_stop_begin() const { return stoping.is_started(); }
  bool is_stoped() const { return stoping.is_complete(); }

  void start_begin() {
    if (stoping.is_started()) {
      throw std::logic_error("begin on stoping process");
    }
    stoping.reset();
    starting.start();
  }

  void start_complete() {
    if (stoping.is_started()) {
      throw std::logic_error("complete on stoping process");
    }
    starting.complete();
    stoping.reset();
  }

  void stop_begin(bool checkTwiceStoping = true) { stoping.start(checkTwiceStoping); }

  void stop_complete(bool checkDoubleStoping = false) {
    starting.reset();
    stoping.complete(checkDoubleStoping);
  }

  void wait_starting() { starting.wait(); }

  void wait_stoping() { stoping.wait(); }

  long_process starting;
  long_process stoping;
};
} // namespace utils
} // namespace nmq
