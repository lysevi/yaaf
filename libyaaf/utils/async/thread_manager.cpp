#include <libyaaf/utils/async/thread_manager.h>
#include <libyaaf/utils/exception.h>
#include <libyaaf/utils/utils.h>

using namespace yaaf::utils::async;

thread_manager::thread_manager(const thread_manager::params_t &params) : _params(params) {
  for (const auto &kv : _params.pools) {
    ENSURE(_pools.find(kv.kind) == _pools.end());
    _pools[kv.kind] = std::make_shared<threads_pool>(kv);
  }
  _stoped = false;
}

void thread_manager::flush() {
  for (auto &kv : _pools) {
    kv.second->flush();
  }
}

task_result_ptr thread_manager::post(const thread_kind_t kind,
                                     const task_wrapper_ptr &task) {
  if (_stoping_begin) {
    return nullptr;
  }
  auto target = _pools.find(kind);
  if (target == _pools.end()) {
    throw MAKE_EXCEPTION("unknow kind.");
  }
  return target->second->post(task);
}

thread_manager::~thread_manager() {
  stop();
}

void thread_manager::stop() {
  _stoping_begin = true;
  if (!_stoped) {
    for (auto &&kv : _pools) {
      kv.second->stop();
    }
    _pools.clear();
    _stoped = true;
  }
}