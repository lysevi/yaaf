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

class LongProcess {
public:
  LongProcess() = delete;

  LongProcess(const std::string &name, bool checkOnDtor_)
      : _name(name), checkOnDtor(checkOnDtor_) {
    _started.store(false);
    _complete.store(false);
  }

  ~LongProcess() {
    if (checkOnDtor && !isComplete()) {
      logger_fatal(_name + " Process was not _completed correctly");
      std::abort();
    }
  }

  bool isStarted() const { return _started.load(); }
  bool isComplete() const { return _complete.load(); }

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

struct Waitable {
  Waitable() : starting("starting", false), stoping("stoping", true) {}

  ~Waitable() {
    if (!isStoped()) {
      logger_fatal("Process was not stopped correctly");
      std::abort();
    }
  }
  bool isStartBegin() const { return starting.isStarted(); }
  bool isStarted() const { return starting.isComplete(); }
  bool isStopBegin() const { return stoping.isStarted(); }
  bool isStoped() const { return stoping.isComplete(); }

  void startBegin() {
    if (stoping.isStarted()) {
      throw std::logic_error("begin on stoping process");
    }
    stoping.reset();
    starting.start();
  }

  void startComplete() {
    if (stoping.isStarted()) {
      throw std::logic_error("complete on stoping process");
    }
    starting.complete();
    stoping.reset();
  }

  void stopBegin(bool checkTwiceStoping = true) { stoping.start(checkTwiceStoping); }

  void stopComplete(bool checkDoubleStoping = false) {
    starting.reset();
    stoping.complete(checkDoubleStoping);
  }

  void waitStarting() { starting.wait(); }

  void waitStoping() { stoping.wait(); }

  LongProcess starting;
  LongProcess stoping;
};
} // namespace utils
} // namespace nmq
