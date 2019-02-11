#pragma once

#include <atomic>
#include <mutex> //for lock_guard
#include <thread>

namespace yaaf {
namespace utils {
namespace async {
// using Locker=std::mutex;
const size_t LOCKER_MAX_TRY = 10;
class locker {
  std::atomic_flag locked = ATOMIC_FLAG_INIT;

public:
  bool try_lock() noexcept {
    for (size_t num_try = 0; num_try < LOCKER_MAX_TRY; ++num_try) {
      if (!locked.test_and_set(std::memory_order_acquire)) {
        return true;
      }
    }
    return false;
  }

  void lock() noexcept {
    while (!try_lock()) {
      std::this_thread::yield();
    }
  }

  void unlock() noexcept { locked.clear(std::memory_order_release); }
};

using Locker_ptr = std::shared_ptr<yaaf::utils::async::locker>;
} // namespace async
} // namespace utils
} // namespace yaaf
