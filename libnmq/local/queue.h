#pragma once

#include <cstdint>
#include <shared_mutex>
#include <vector>

namespace nmq {
namespace local {

template <class T> struct Result {
  bool ok;
  T result;

  static Result<T> False() { return Result{false, T()}; }
  static Result<T> Ok(T t) { return Result{true, t}; }
};

template <class T, class Cont = std::vector<T>> class Queue {
public:
  Queue(size_t capacity) : _values(capacity) { _cap = capacity; }

  bool tryPush(T v) {
    std::lock_guard<std::shared_mutex> lg(_locker);
    if (_pos == _cap) {
      return false;
    } else {
      _values[_pos++] = v;
      return true;
    }
  }

  Result<T> tryPop() {
    std::lock_guard<std::shared_mutex> lg(_locker);
    if (_pos == 0) {
      return Result<T>::False();
    } else {
      _pos--;
      return Result<T>::Ok(_values[_pos]);
    }
  }

  size_t capacity() const { return (size_t)(_cap); }

  bool empty() const {
    std::shared_lock<std::shared_mutex> slg(_locker);
    return _pos == 0;
  }

private:
  mutable std::shared_mutex _locker;
  size_t _cap;
  size_t _pos = 0;
  Cont _values;
};
} // namespace local
} // namespace nmq