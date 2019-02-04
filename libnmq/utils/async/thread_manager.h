#pragma once

#include <libnmq/exports.h>
#include <libnmq/utils/async/thread_pool.h>
#include <libnmq/utils/utils.h>
#include <unordered_map>

namespace nmq {
namespace utils {
namespace async {

class thread_manager : public utils::non_copy {

public:
  struct params_t {
    std::vector<threads_pool::params_t> pools;
    params_t(std::vector<threads_pool::params_t> _pools) { pools = _pools; }
  };
  EXPORT static void start(const params_t &params);
  EXPORT static void stop();
  EXPORT static thread_manager *instance();

  EXPORT ~thread_manager();
  EXPORT void flush();
  task_result_ptr post(const THREAD_KINDS kind,
                      const std::shared_ptr<async_task_wrapper> &task) {
    return this->post((thread_kind_t)kind, task);
  }
  EXPORT task_result_ptr post(const thread_kind_t kind, const async_task_wrapper_ptr &task);

  size_t active_works() {
    size_t res = 0;
    for (const auto &kv : _pools) {
      res += kv.second->active_works();
    }
    return res;
  }

private:
  thread_manager(const params_t &params);

private:
  static thread_manager *_instance;
  bool _stoped;
  params_t _params;
  std::unordered_map<thread_kind_t, std::shared_ptr<threads_pool>> _pools;
};
}
}
}
