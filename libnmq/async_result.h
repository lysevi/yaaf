#pragma once

#include <libnmq/errors.h>
#include <libnmq/utils/async/locker.h>
#include <libnmq/types.h>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace nmq {

struct async_operation_handler {
  id_t id;
  std::shared_ptr<utils::async::locker> locker;

  void wait() { locker->lock(); }

  void mark_as_finished() { locker->unlock(); }

  static async_operation_handler make_new(id_t id_) {
    async_operation_handler result{id_};
    result.locker = std::make_shared<utils::async::locker>();
    result.locker->lock();
    return result;
  }
};

class ao_supervisor {
public:
  ao_supervisor() = default;

  async_operation_handler make_async_result() {
    std::lock_guard<std::shared_mutex> lg(_asyncOperations_locker);
    ++__asyncOperationsid;
    auto result = async_operation_handler::make_new(__asyncOperationsid);
    _asyncOperations[__asyncOperationsid] = result;
    return result;
  }

  void mark_operation_as_finished(id_t id) {
    std::lock_guard<std::shared_mutex> lg(_asyncOperations_locker);
    auto ao = _asyncOperations.find(id);
    if (ao == _asyncOperations.end()) {
      return;
    }
    ao->second.mark_as_finished();
    _asyncOperations.erase(id);
  }

  void wait_all_async_operations() {
    for (;;) {
      bool empty = false;
      _asyncOperations_locker.lock_shared();
      empty = _asyncOperations.empty();
      _asyncOperations_locker.unlock_shared();
      if (empty) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

private:
  std::shared_mutex _asyncOperations_locker;
  id_t __asyncOperationsid = 0;
  std::unordered_map<id_t, async_operation_handler> _asyncOperations;
};
} // namespace nmq