#include <libnmq/context.h>
#include <libnmq/utils/logger.h>
#include <unordered_set>

using namespace nmq;
using namespace nmq::utils::logging;
using namespace nmq::utils::async;

namespace {
const thread_kind_t USER = 1;
const thread_kind_t SYSTEM = 2;
} // namespace

context::params_t context::params_t::defparams() {
  context::params_t r{};
  r.user_threads = 1;
  r.sys_threads = 1;
  return r;
}

std::shared_ptr<context> context::make_context() {
  return context::make_context(params_t::defparams());
}

std::shared_ptr<context> context::make_context(context::params_t p) {
  return std::make_shared<context>(p);
}

context::context(context::params_t p) : _params(p) {

  std::vector<threads_pool::params_t> pools{
      threads_pool::params_t(_params.user_threads, USER),
      threads_pool::params_t(_params.sys_threads, SYSTEM)};
  thread_manager::params_t tparams(pools);

  _thread_manager = std::make_unique<thread_manager>(tparams);
  task t = [this](const thread_info &) {
    this->mailbox_worker();
    return CONTINUATION_STRATEGY::REPEAT;
  };
  auto wrapped = wrap_task(t);
  _thread_manager->post(SYSTEM, wrapped);
}

context ::~context() {
  logger_info("context: stoping....");
  _thread_manager->stop();
  _thread_manager = nullptr;
  logger_info("context: stoped");
}

actor_address context::add_actor(actor_for_delegate::delegate_t f) {
  actor_ptr aptr = std::make_shared<actor_for_delegate>(f);
  return add_actor(aptr);
}

actor_address context::add_actor(actor_ptr a) {
  std::lock_guard<std::shared_mutex> lg(_locker);

  auto new_id = id_t(_next_actor_id++);

  logger_info("context: add actor #", new_id);
  actor_address result{new_id, this};
  a->on_init();
  a->set_self_addr(result);
  _actors[new_id] = a;
  _mboxes[new_id] = std::make_shared<mailbox>();

  task t = [a](const thread_info &tinfo) {
    UNUSED(tinfo);
    TKIND_CHECK(tinfo.kind, USER);
    a->on_start();
    return CONTINUATION_STRATEGY::SINGLE;
  };

  _thread_manager->post(USER, wrap_task(t));

  return result;
}

actor_ptr context::get_actor(id_t id) {
  std::shared_lock<std::shared_mutex> lg(_locker);
  logger_info("context: get actor #", id);
  auto it = _actors.find(id);
  if (it != _actors.end()) { // actor may be stopped
    return it->second;
  } else {
    return nullptr;
  }
}

void context::send(actor_address &addr, envelope msg) {
  std::shared_lock<std::shared_mutex> lg(_locker);
  logger_info("context: send to #", addr.get_id());
  auto it = _mboxes.find(addr.get_id());
  if (it != _mboxes.end()) { // actor may be stopped
    it->second->push(msg);
  }
}

void context::stop_actor(actor_address &addr) {
  std::lock_guard<std::shared_mutex> lg(_locker);
  auto it = _actors.find(addr.get_id());
  if (it != _actors.end()) { // double-stop protection;
    it->second->on_stop();
    _mboxes.erase(addr.get_id());
    _actors.erase(it);
  }
}

void context::mailbox_worker() {
  logger_info("context: mailbox_worker");

  _locker.lock_shared();
  std::unordered_set<id_t> to_remove;

  for (auto kv : _mboxes) {
    auto mb = kv.second;
    if (!mb->empty()) {
      auto it = _actors.find(kv.first);
      if (it == _actors.end()) {
        envelope e;
        mb->try_pop(e);
        to_remove.insert(kv.first);
        continue;
      }

      auto target_actor = _actors[kv.first];

      if (target_actor->try_lock()) {
        if (mb->empty()) {
          target_actor->reset_busy();
        } else {
          task t = [target_actor, mb](const thread_info &tinfo) {
            UNUSED(tinfo);
            TKIND_CHECK(tinfo.kind, USER);
            try {
              target_actor->apply(*mb);
            } catch (std::exception &ex) {
              logger_warn(ex.what());
            }
            return CONTINUATION_STRATEGY::SINGLE;
          };

          _thread_manager->post(USER, wrap_task(t));
        }
      }
    }
  }

  _locker.unlock_shared();

  if (!to_remove.empty()) {
    std::lock_guard<std::shared_mutex> lg(_locker);
    for (auto v : to_remove) {
      _mboxes.erase(v);
    }
  }
  // TODO need a sleeping?
}