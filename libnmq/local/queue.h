#pragma once

#include <cstdint>
#include <shared_mutex>
#include <vector>

namespace nmq {
namespace local {

template <class T> struct result_t {
  bool ok;
  T value;

  static result_t<T> False() { return result_t{false, T()}; }
  static result_t<T> Ok(T t) { return result_t{true, t}; }
};

template <class T, class Cont = std::vector<T>> class queue {
public:
  queue(size_t capacity) : _values(capacity) { _cap = capacity; }

  bool try_push(T v) noexcept {
    std::lock_guard<std::shared_mutex> lg(_locker);
    if (_pos == _cap) {
      return false;
    } else {
      _values[_pos++] = v;
      return true;
    }
  }

  result_t<T> try_pop() noexcept {
    std::lock_guard<std::shared_mutex> lg(_locker);
    if (_pos == 0) {
      return result_t<T>::False();
    } else {
      _pos--;
      return result_t<T>::Ok(_values[_pos]);
    }
  }

  size_t capacity() const noexcept { return (size_t)(_cap); }

  bool empty() const noexcept {
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