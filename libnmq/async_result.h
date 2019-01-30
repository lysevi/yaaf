#pragma once

#include <libnmq/errors.h>
#include <libnmq/utils/async/locker.h>
#include <libnmq/types.h>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace nmq {

struct AsyncOperationResult {
  Id id;
  std::shared_ptr<utils::async::locker> locker;

  void wait() { locker->lock(); }

  void markAsFinished() { locker->unlock(); }

  static AsyncOperationResult makeNew(Id id_) {
    AsyncOperationResult result;
    result.locker = std::make_shared<utils::async::locker>();
    result.locker->lock();
    result.id = id_;
    return result;
  }
};

class AsyncOperationsStorage {
public:
  AsyncOperationsStorage() = default;

  AsyncOperationResult makeAsyncResult() {
    std::lock_guard<std::shared_mutex> lg(_asyncOperations_locker);
    __asyncOperationsId++;
    auto result = AsyncOperationResult::makeNew(__asyncOperationsId);
    _asyncOperations[__asyncOperationsId] = result;
    return result;
  }

  void markOperationAsFinished(Id id) {
    std::lock_guard<std::shared_mutex> lg(_asyncOperations_locker);
    auto ao = _asyncOperations.find(id);
    if (ao == _asyncOperations.end()) {
      return;
    }
    ao->second.markAsFinished();
    _asyncOperations.erase(id);
  }

  void waitAllAsyncOperations() {
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
  Id __asyncOperationsId = 0;
  std::unordered_map<Id, AsyncOperationResult> _asyncOperations;
};
} // namespace nmq