#include <libyaaf/utils/async/thread_pool.h>
#include <libyaaf/utils/logger.h>
#include <algorithm>

using namespace yaaf::utils;
using namespace yaaf::utils::logging;
using namespace yaaf::utils::async;

threads_pool::threads_pool(const params_t &p) : _params(p) {
  ENSURE(_params.threads_count > 0);
  _stop_flag = false;
  _is_stoped = false;
  _task_runned = size_t(0);
  _threads.resize(_params.threads_count);
  for (size_t i = 0; i < _params.threads_count; ++i) {
    _threads[i] = std::thread{&threads_pool::_pool_logic, this, i};
  }
}

threads_pool::~threads_pool() {
  if (!_is_stoped) {
    stop();
  }
}

task_result_ptr threads_pool::post(const task_wrapper_ptr &task) {
  if (this->_is_stoped) {
    return nullptr;
  }
  push_task(task);
  return task->result();
}

void threads_pool::stop() {
  {
    std::unique_lock<std::shared_mutex> lock(_queue_mutex);
    _stop_flag = true;
  }
  for (std::thread &worker : _threads){
    _condition.notify_all();
    worker.join();
  }
  _is_stoped = true;
}

void threads_pool::flush() {
  while (true) {
    _condition.notify_one();
    std::unique_lock<std::shared_mutex> lock(_queue_mutex);
    if (!_in_queue.empty()) {
      auto is_workers_only = std::all_of(_in_queue.begin(), _in_queue.end(),
                                         [](const task_wrapper_ptr &t) {
                                           return t->priority == TASK_PRIORITY::WORKER;
                                         });
      if (!is_workers_only) {
        continue;
      }
    }
    if (_task_runned.load() == size_t(0)) {
      break;
    } else {
      std::this_thread::yield();
    }
  }
}

void threads_pool::push_task(const task_wrapper_ptr &at) {
  {
    std::unique_lock<std::shared_mutex> lock(_queue_mutex);
    _in_queue.push_back(at);
  }
  _condition.notify_all();
}

void threads_pool::_pool_logic(size_t num) {
  thread_info ti{};
  ti.kind = _params.kind;
  ti.thread_number = num;

  while (!_stop_flag) {
    std::shared_ptr<task_wrapper> task = nullptr;

    {
      std::unique_lock<std::shared_mutex> lock(_queue_mutex);
      this->_condition.wait(
          lock, [this] { return this->_stop_flag || !this->_in_queue.empty(); });
      if (this->_stop_flag) {
        return;
      }
      _task_runned++;
      for (auto v : _in_queue) {
        if (task == nullptr) {
          task = v;
          if (task->priority != TASK_PRIORITY::WORKER) {
            break;
          }
        } else {
          if (v->priority < task->priority) {
            task = v;
            break;
          }
        }
      }
      this->_in_queue.erase(std::find(_in_queue.begin(), _in_queue.end(), task));
    }

    // if queue is empty and task is coroutine, it will be run in cycle.
    while (true) {
      auto need_continue = task->apply(ti);
      if (need_continue == CONTINUATION_STRATEGY::SINGLE) {
        break;
      }
      _queue_mutex.lock_shared();
      if (!_in_queue.empty() || this->_stop_flag ||
          task->priority == TASK_PRIORITY::WORKER) {
        _queue_mutex.unlock_shared();
        push_task(task);
        break;
      }
      _queue_mutex.unlock_shared();
    }
    --_task_runned;
  }
}
