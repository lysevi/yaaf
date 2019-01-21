#pragma once

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
    _cap = int64_t(capacity);
  }

  void addCallback(Callback clbk) { _clbks.push_back(clbk); }

  bool tryPush(T v) {
    for (;;) {
      auto i = _position.load();

      if (i < _cap - 1) {

        auto new_i = i + 1;
        if (_position.compare_exchange_strong(i, new_i)) {
          _values[new_i] = v;
          for (auto &c : _clbks) {
            c();
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
    if (i < 0) {
      return std::tuple<bool, T>(false, T());
    }
    auto new_i = i - 1;
    if (_position.compare_exchange_strong(i, new_i)) {
      return std::tuple<bool, T>(true, _values[i]);
    }
    return std::tuple<bool, T>(false, T());
  }

  size_t capacity() const { return (size_t)(_cap); }

private:
  std::atomic_int64_t _position;
  int64_t _cap;
  Cont _values;
  std::list<Callback> _clbks;
};
} // namespace lockfree
} // namespace nmq