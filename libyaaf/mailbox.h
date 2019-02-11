#pragma once

#include <libyaaf/envelope.h>
#include <deque>
#include <shared_mutex>

namespace yaaf {
class mailbox {
public:
  bool empty() const {
    std::shared_lock<std::shared_mutex> lg(_locker);
    return _dqueue.empty();
  }

  size_t size() const {
    std::shared_lock<std::shared_mutex> lg(_locker);
    return _dqueue.size();
  }

  void push(const envelope &e) {
    std::lock_guard<std::shared_mutex> lg(_locker);
    _dqueue.emplace_back(e);
  }

  void push(const envelope &&e) {
    std::lock_guard<std::shared_mutex> lg(_locker);
    _dqueue.emplace_back(std::move(e));
  }

  template <class T> void push(T &&t, const actor_address &sender) {
    std::lock_guard<std::shared_mutex> lg(_locker);
    envelope ep;
    ep.payload = std::forward<T>(t);
    ep.sender = sender;
    _dqueue.emplace_back(ep);
  }

  bool try_pop(envelope &out) {
    std::lock_guard<std::shared_mutex> lg(_locker);
    if (_dqueue.empty()) {
      return false;
    } else {
      out = _dqueue.front();
      _dqueue.pop_front();
      return true;
    }
  }

private:
  mutable std::shared_mutex _locker;
  std::deque<envelope> _dqueue;
};
} // namespace yaaf
