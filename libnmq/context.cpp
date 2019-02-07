#include <libnmq/context.h>
#include <libnmq/utils/logger.h>
#include <unordered_set>

using namespace nmq;
using namespace nmq::utils::logging;
using namespace nmq::utils::async;

std::atomic_size_t context::_ctx_id = {0};

namespace {
const thread_kind_t USER = 1;
const thread_kind_t SYSTEM = 2;

class user_context : public abstract_context {
public:
  user_context(std::weak_ptr<context> ctx, const actor_address &addr, std::string name)
      : _ctx(ctx), _addr(addr), _name(name) {}

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

  std::string name() const override { return _name; }

private:
  std::weak_ptr<context> _ctx;
  actor_address _addr;
  std::string _name;
};
} // namespace

context::params_t context::params_t::defparams() {
  context::params_t r{};
  r.user_threads = 1;
  r.sys_threads = 1;
  return r;
}

std::shared_ptr<context> context::make_context(std::string name) {
  return context::make_context(params_t::defparams(), name);
}

std::shared_ptr<context> context::make_context(const params_t &params, std::string name) {
  auto result = std::make_shared<context>(params, name);
  result->start();
  return result;
}

context::context(const context::params_t &p, std::string name)
    : abstract_context(), _params(p) {

  std::vector<threads_pool::params_t> pools{
      threads_pool::params_t(_params.user_threads, USER),
      threads_pool::params_t(_params.sys_threads, SYSTEM)};
  thread_manager::params_t tparams(pools);
  if (name == "") {
    auto self_id = _ctx_id.fetch_add(1);
    _name = "system_" + std::to_string(self_id);
  } else {
    _name = name;
  }
  _thread_manager = std::make_unique<thread_manager>(tparams);
}

context ::~context() {
  logger_info("context: stoping....");
  _thread_manager->stop();
  _thread_manager = nullptr;
  for (auto kv : _actors) {
    kv.second->actor->on_stop();
  }
  _actors.clear();
  _mboxes.clear();
  logger_info("context: stoped");
}

void context::start() {
  task t = [this](const thread_info &) {
    this->mailbox_worker();
    return CONTINUATION_STRATEGY::REPEAT;
  };
  auto wrapped = wrap_task(t);
  _thread_manager->post(SYSTEM, wrapped);
}

std::string context::name() const {
  return _name;
}
actor_address context::add_actor(const actor_ptr a) {
  actor_address empty;
  return add_actor(empty, a);
}

actor_address context::add_actor(const actor_address &parent, const actor_ptr a) {
  auto new_id = id_t(_next_actor_id++);

  logger_info("context: add actor #", new_id);
  auto self = shared_from_this();

  auto d = std::make_shared<inner::description>();
  auto ucname = name() + "/_usercontext_" + std::to_string(new_id.value);
  d->usrcont = std::make_shared<user_context>(self, actor_address{new_id}, ucname);
  a->set_context(d->usrcont);

  auto settings = actor_settings::defsettings();
  if (!parent.empty()) {
    auto parent_ac = _actors[parent.get_id()];
    d->parent = parent.get_id();
    settings = parent_ac->settings;
  }

  d->actor = a;
  d->settings = a->on_init(settings);

  actor_address result{new_id};
  a->set_self_addr(result);

  {
    std::lock_guard<std::shared_mutex> lg(_locker);
    if (!parent.empty()) {
      auto parent_desc = _actors[parent.get_id()];
      parent_desc->children.insert(new_id);
    }

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
    return it->second->actor;
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
  return stop_actor_impl_safety(addr.get_id(), actor_stopping_reason::MANUAL);
}

void context::stop_actor_impl_safety(const actor_address &addr,
                                     actor_stopping_reason reason) {
  std::lock_guard<std::shared_mutex> lg(_locker);
  return stop_actor_impl(addr.get_id(), reason);
}

void context::stop_actor_impl(const actor_address &addr, actor_stopping_reason reason) {
  auto id = addr.get_id();
  logger_info("context: stop #", id);
  auto it = _actors.find(id);
  if (it != _actors.end()) { // double-stop protection;
    auto desc = it->second;

    for (auto &&c : desc->children) {
      stop_actor_impl(c, reason);
    }

    desc->actor->on_stop();
    if (!desc->parent.empty()) {
      auto parent = _actors.find(desc->parent);
      if (parent != _actors.end()) {
        parent->second->actor->on_child_stopped(addr, reason);
      }
    }
    _mboxes.erase(id);
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
      auto id = it->first;
      auto target_actor_description = it->second;

      if (target_actor_description->actor->try_lock()) {
        if (mb->empty()) {
          target_actor_description->actor->reset_busy();
        } else {
          actor_ptr parent = nullptr;
          if (!target_actor_description->parent.empty()) {
            parent = _actors[target_actor_description->parent]->actor;
          }

          task t = [this, target_actor_description, id, parent,
                    mb](const thread_info &tinfo) {
            TKIND_CHECK(tinfo.kind, USER);
            try {
              target_actor_description->actor->apply(*mb);
              if (parent != nullptr) {
                parent->on_child_status(actor_address{id}, actor_status_kinds::NORMAL);
              }
            } catch (std::exception &ex) {
              logger_warn(ex.what());
              if (target_actor_description->settings.stop_on_any_error) {
                this->stop_actor_impl_safety(actor_address(id),
                                             actor_stopping_reason::EXCEPT);
              }
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