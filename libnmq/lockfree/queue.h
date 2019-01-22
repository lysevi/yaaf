#pragma once

#include <libnmq/utils/async/locker.h>
#include <atomic>
#include <cstdint>
#include <functional>
#include <list>
#include <tuple>
#include <type_traits>

namespace nmq {
namespace lockfree {
template <class T, class Cont = std::vector<T>> class FixedQueue {
public:
  using Callback = std::function<void()>;
  FixedQueue(size_t capacity) : _position(-1), _values(capacity) {
    static_assert(std::is_default_constructible_v<T>,
                  "T is not std::is_default_constructible_v");
    static_assert(std::is_trivially_copyable_v<T>, "T is not std::is_trivially_copyable");
    _cap = int64_t(capacity);
  }

  bool tryAddCallback(Callback clbk) {
    if (_locker.try_lock()) {
      _clbks.push_back(clbk);
      _locker.unlock();
      return true;
    }
    return false;
  }

  bool tryPush(T v) {
    for (;;) {
      auto i = _position.load();

      if (i < _cap - 1) {

        auto new_i = i + 1;
        if (_position.compare_exchange_strong(i, new_i)) {
          _values[new_i] = v;

          if (!_clbks.empty()) {
            for (auto &c : _clbks) {
              c();
            }
          }
          return true;
        }
      } else {
        return false;
      }
    }
  }

  std::tuple<bool, T> tryPop() {

    auto i = _position.load();
    while (i >= 0) {
      i = _position.load();
      T result{_values[i]};
      auto new_i = i - 1;
      if (i >= 0 && _position.compare_exchange_strong(i, new_i)) {
        return std::tuple<bool, T>(true, result);
      }
    }
    return std::tuple<bool, T>(false, T());
  }

  size_t capacity() const { return (size_t)(_cap); }

  bool empty() const { return !_position.load() >= 0; }

private:
  std::atomic_int64_t _position;
  int64_t _cap;
  Cont _values;

  utils::async::locker _locker;
  std::list<Callback> _clbks;
};
} // namespace lockfree
} // namespace nmq