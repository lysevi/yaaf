#include <libnmq/context.h>
#include <libnmq/utils/logger.h>
#include <unordered_set>

using namespace nmq;
using namespace nmq::utils::logging;
using namespace nmq::utils::async;

namespace {
const thread_kind_t USER = 1;
const thread_kind_t SYSTEM = 2;

class user_context : public abstract_context {
public:
  user_context(std::weak_ptr<context> ctx, const actor_address &addr)
      : _ctx(ctx), _addr(addr) {}

  actor_address add_actor(const actor_ptr a) override {
    if (auto c = _ctx.lock()) {
      return c->add_actor(_addr, a);
    }
    THROW_EXCEPTION("context is nullptr");
  }

  void send_envelope(const actor_address &target, envelope msg) override {
    if (auto c = _ctx.lock()) {
      msg.sender = _addr;
      return c->send_envelope(target, msg);
    }
    THROW_EXCEPTION("context is nullptr");
  }

  void stop_actor(const actor_address &addr) {
    if (auto c = _ctx.lock()) {
      return c->stop_actor(addr);
    }
    THROW_EXCEPTION("context is nullptr");
  }

  actor_ptr get_actor(id_t id) {
    if (auto c = _ctx.lock()) {
      return c->get_actor(id);
    }
    THROW_EXCEPTION("context is nullptr");
  }

private:
  std::weak_ptr<context> _ctx;
  actor_address _addr;
};
} // namespace

context::params_t context::params_t::defparams() {
  context::params_t r{};
  r.user_threads = 1;
  r.sys_threads = 1;
  return r;
}

abstract_context ::~abstract_context() {
  logger_info("~abstract_context");
}

actor_address abstract_context::add_actor(const actor_for_delegate::delegate_t f) {
  return make_actor<actor_for_delegate>(f);
}

std::shared_ptr<context> context::make_context() {
  return context::make_context(params_t::defparams());
}

std::shared_ptr<context> context::make_context(const params_t &params) {
  return std::make_shared<context>(params);
}

context::context(const context::params_t &p) : abstract_context(), _params(p) {

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
  for (auto kv : _actors) {
    kv.second.actor->on_stop();
  }
  _actors.clear();
  _mboxes.clear();
  logger_info("context: stoped");
}

actor_address context::add_actor(const actor_ptr a) {
  actor_address empty;
  return add_actor(empty, a);
}

actor_address context::add_actor(const actor_address &parent, const actor_ptr a) {
  auto new_id = id_t(_next_actor_id++);

  logger_info("context: add actor #", new_id);
  auto self = shared_from_this();

  inner::description d;
  d.usrcont = std::make_shared<user_context>(self, actor_address{new_id});
  d.actor = a;
  d.settings = a->on_init(actor_settings::defsettings());
  if (!parent.empty()) {
    d.parent = parent.get_id();
  }

  actor_address result{new_id};
  a->set_self_addr(result);
  a->set_context(d.usrcont);

  {
    std::lock_guard<std::shared_mutex> lg(_locker);
    _actors[new_id] = d;
    _mboxes[new_id] = std::make_shared<mailbox>();
  }

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
    return it->second.actor;
  } else {
    return nullptr;
  }
}

void context::send_envelope(const actor_address &target, envelope msg) {
  std::shared_lock<std::shared_mutex> lg(_locker);
  logger_info("context: send to #", target.get_id());
  auto it = _mboxes.find(target.get_id());
  if (it != _mboxes.end()) { // actor may be stopped
    it->second->push(msg);
  }
}

void context::stop_actor(const actor_address &addr) {
  std::lock_guard<std::shared_mutex> lg(_locker);
  auto it = _actors.find(addr.get_id());
  if (it != _actors.end()) { // double-stop protection;
    it->second.actor->on_stop();
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

      auto target_actor = it->second.actor;

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