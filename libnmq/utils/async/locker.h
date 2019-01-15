#pragma once

#include <atomic>
#include <mutex> //for lock_guard
#include <thread>

namespace nmq {
namespace utils {
namespace async {
// using Locker=std::mutex;
const size_t LOCKER_MAX_TRY = 10;
class locker {
  std::atomic_flag locked = ATOMIC_FLAG_INIT;

public:
  void lock() {
    size_t num_try = 0;
    while (locked.test_and_set(std::memory_order_acquire)) {
      num_try++;
      if (num_try >= LOCKER_MAX_TRY) {
        num_try = 0;
        std::this_thread::yield();
      }
    }
  }
  void unlock() { locked.clear(std::memory_order_release); }
};

using Locker_ptr = std::shared_ptr<nmq::utils::async::locker>;
} // namespace async
} // namespace utils
} // namespace nmq
