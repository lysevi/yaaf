#include <libnmq/utils/async/thread_pool.h>
#include <libnmq/utils/logger.h>
#include <algorithm>

using namespace nmq::utils;
using namespace nmq::utils::logging;
using namespace nmq::utils::async;

async_task_wrapper::async_task_wrapper(async_task &t, const std::string &_function,
                             const std::string &file, int line) {
  priority = TASK_PRIORITY::DEFAULT;
  _task = t;
  _parent_function = _function;
  _code_file = file;
  _code_line = line;
  _result = std::make_shared<task_result>();
}

async_task_wrapper::async_task_wrapper(async_task &t, const std::string &_function,
                             const std::string &file, int line, TASK_PRIORITY p)
    : async_task_wrapper(t, _function, file, line) {
  this->priority = p;
}

RUN_STRATEGY async_task_wrapper::apply(const thread_info &ti) {
  _tinfo.kind = ti.kind;
  _tinfo.thread_number = ti.thread_number;

  if (worker() == RUN_STRATEGY::SINGLE) {
    _result->unlock();
    return RUN_STRATEGY::SINGLE;
  }

  return RUN_STRATEGY::REPEAT;
}

RUN_STRATEGY async_task_wrapper::worker() {
  try {
    return _task(this->_tinfo);
  } catch (std::exception &ex) {
    logger_fatal("engine: *** async task exception:", _parent_function,
                 " file:", _code_file, " line:", _code_line);
    logger_fatal("engine: *** what:", ex.what());
    throw;
  }
}

task_result_ptr async_task_wrapper::result() const {
  return _result;
}
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

task_result_ptr threads_pool::post(const async_task_wrapper_ptr &task) {
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
  _condition.notify_all();
  for (std::thread &worker : _threads)
    worker.join();
  _is_stoped = true;
}

void threads_pool::flush() {
  while (true) {
    _condition.notify_one();
    std::unique_lock<std::shared_mutex> lock(_queue_mutex);
    if (!_in_queue.empty()) {
      auto is_workers_only =
          std::all_of(_in_queue.begin(), _in_queue.end(), [](const async_task_wrapper_ptr &t) {
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

void threads_pool::push_task(const async_task_wrapper_ptr &at) {
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
    std::shared_ptr<async_task_wrapper> task = nullptr;

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
      if (need_continue == RUN_STRATEGY::SINGLE) {
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
