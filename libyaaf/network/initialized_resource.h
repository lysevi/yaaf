#pragma once

#include <atomic>
#include <cassert>
#include <exception>
#include <iostream>

namespace yaaf {
namespace network {
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
      std::cerr << _name + " Process was not _completed correctly" << std::endl;
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
    assert(_started);
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

struct initialized_resource {
  initialized_resource() : starting("starting", false), stoping("stoping", true) {}

  ~initialized_resource() {
    if (!is_stoped()) {
      std::cerr << " Process was not _completed correctly" << std::endl;
      std::abort();
    }
  }
  bool is_initialisation_begin() const { return starting.is_started(); }
  bool is_started() const { return starting.is_complete(); }
  bool is_stopping_started() const { return stoping.is_started(); }
  bool is_stoped() const { return stoping.is_complete(); }

  void initialisation_begin() {
    if (stoping.is_started()) {
      throw std::logic_error("begin on stoping process");
    }
    stoping.reset();
    starting.start();
  }

  void initialisation_complete() {
    if (stoping.is_started()) {
      throw std::logic_error("complete on stoping process");
    }
    starting.complete();
    stoping.reset();
  }

  void stopping_started(bool checkTwiceStoping = true) {
    stoping.start(checkTwiceStoping);
  }

  void stopping_completed(bool checkDoubleStoping = false) {
    starting.reset();
    stoping.complete(checkDoubleStoping);
  }

  void wait_starting() { starting.wait(); }

  void wait_stoping() { stoping.wait(); }

  long_process starting;
  long_process stoping;
};
} // namespace network
} // namespace yaaf
