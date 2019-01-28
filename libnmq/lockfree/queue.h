#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <type_traits>

namespace nmq {
namespace lockfree {

template <class T> struct Result {
  bool ok;
  T result;

  static Result<T> False() { return Result{false, T()}; }
  static Result<T> Ok(T t) { return Result{true, t}; }
};

template <class T, class Cont = std::vector<T>> class Queue {
public:

  Queue(size_t capacity) : _position(-1), _values(capacity) {
    static_assert(std::is_default_constructible_v<T>,
                  "T is not std::is_default_constructible_v");
    static_assert(std::is_copy_constructible_v<T>,
                  "T is not std::is_copy_constructible_v");
    _cap = int64_t(capacity);
  }

  bool tryPush(T v) {
    for (;;) {
      auto i = _position.load();

      if (i < _cap - 1) {

        auto new_i = i + 1;
        if (_position.compare_exchange_strong(i, new_i)) {
          _values[new_i] = v;
          return true;
        }
      } else {
        return false;
      }
    }
  }

  Result<T> tryPop() {
    auto i = _position.load();
    while (i >= 0) {
      i = _position.load();
      T result{_values[i]};
      auto new_i = i - 1;
      if (i >= 0 && _position.compare_exchange_strong(i, new_i)) {
        return Result<T>::Ok(result);
      }
    }
    return Result<T>::False();
  }

  size_t capacity() const { return (size_t)(_cap); }

  bool empty() const { return !(_position.load() >= 0); }

private:
  std::atomic_int64_t _position;
  int64_t _cap;
  Cont _values;
};
} // namespace lockfree
} // namespace nmq