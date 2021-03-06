#include <libyaaf/actor_address.h>
#include <libyaaf/context.h>
#include <libyaaf/utils/logger.h>
#include <unordered_set>

using namespace yaaf;
using namespace yaaf::utils::logging;
using namespace yaaf::utils::async;

std::atomic_size_t context::_ctx_id = {0};

namespace {
const thread_kind_t USER = 1;
const thread_kind_t SYSTEM = 2;
const thread_kind_t NETWORK = 3;

class user_context final : public abstract_context {
public:
  user_context(std::weak_ptr<context> ctx, const actor_address &addr, std::string name)
      : _ctx(ctx), _addr(addr), _name(name) {
    ENSURE(_addr.get_pathname() != "null");
    ENSURE(_addr.get_pathname() != "");
    ENSURE(_name != "null");
    ENSURE(_name != _addr.get_pathname());
  }

  actor_address add_actor(const std::string &actor_name, const actor_ptr a) override {
    if (auto c = _ctx.lock()) {
      if (!c->is_stopping_begin()) {
        return c->add_actor(actor_name, _addr, a);
      }
    }
    return actor_address();
    // THROW_EXCEPTION("context is nullptr");
  }

  void send_envelope(const actor_address &target, const envelope &e) override {
    if (auto c = _ctx.lock()) {
      if (!c->is_stopping_begin()) {
        envelope cp{e.payload, _addr};
        c->send_envelope(target, std::move(cp));
      }
    }
    // THROW_EXCEPTION("context is nullptr");
  }

  void send_envelope(const actor_address &target, const envelope &&e) override {
    if (auto c = _ctx.lock()) {
      if (!c->is_stopping_begin()) {
        envelope cp{e.payload, _addr};
        c->send_envelope(target, std::move(cp));
      }
    }
  }

  void stop_actor(const actor_address &addr) {
    if (auto c = _ctx.lock()) {
      if (!c->is_stopping_begin()) {
        c->stop_actor(addr);
      }
    }
    // THROW_EXCEPTION("context is nullptr");
  }

  actor_weak get_actor(const actor_address &addr) const override {
    actor_weak result;
    if (auto c = _ctx.lock()) {
      if (!c->is_stopping_begin()) {
        result = c->get_actor(addr);
      }
    }
    return result;
    // THROW_EXCEPTION("context is nullptr");
  }

  actor_weak get_actor(const std::string &name) const override {
    actor_weak result;
    if (auto c = _ctx.lock()) {
      if (!c->is_stopping_begin()) {
        result = c->get_actor(name);
      }
    }
    return result;
  }

  actor_address get_address(const std::string &name) const override {
    actor_address result;
    if (auto c = _ctx.lock()) {
      result = c->get_address(name);
    }
    return result;
  }

  std::string name() const override { return _name; }

  void create_exchange(const std::string &name) override {
    if (auto c = _ctx.lock()) {
      c->create_exchange(_addr, name);
    }
  }

  void subscribe_to_exchange(const std::string &name) override {
    if (auto c = _ctx.lock()) {
      c->subscribe_to_exchange(_addr, name);
    }
  }

  void publish_to_exchange(const std::string &exchange, const envelope &e) override {
    if (auto c = _ctx.lock()) {
      envelope cp{e.payload, _addr};
      c->publish_to_exchange(exchange, std::move(cp));
    }
  }
  void publish_to_exchange(const std::string &exchange, const envelope &&e) override {
    if (auto c = _ctx.lock()) {
      envelope cp{std::move(e.payload), _addr};
      c->publish_to_exchange(exchange, std::move(cp));
    }
  }

  bool exchange_exists(const std::string &name) const {
    if (auto c = _ctx.lock()) {
      return c->exchange_exists(name);
    } else {
      return false;
    }
  }

private:
  std::weak_ptr<context> _ctx;
  actor_address _addr;
  std::string _name;
};

class usr_actor final : public base_actor {
public:
  void action_handle(const envelope &e) { UNUSED(e); }
};

class sys_actor final : public base_actor {
public:
  void action_handle(const envelope &e) { UNUSED(e); }
};

class root_actor final : public base_actor {
public:
  void action_handle(const envelope &e) { UNUSED(e); }
};
} // namespace

context::params_t context::params_t::defparams() {
  context::params_t r{};
  r.user_threads = 1;
  r.sys_threads = 1;
#if YAAF_NETWORK_ENABLED
  r.network_threads = 1;
#endif
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
#ifdef YAAF_NETWORK_ENABLED
  pools.emplace_back(_params.network_threads, NETWORK);
#endif
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
  logger_info("context: ~context()");
  if (_thread_manager != nullptr) {
    stop();
  }
}

void context::start() {
  logger_info("context: start...");

  auto t = sys_post([this]() { this->mailbox_worker(); }, CONTINUATION_STRATEGY::REPEAT);
  t = nullptr;

  _root = make_actor<root_actor>("root");
  _usr_root = this->add_actor("usr", _root, std::make_shared<usr_actor>());
  _sys_root = this->add_actor("sys", _root, std::make_shared<sys_actor>());

  network_init();
}

void context::stop() {
  logger_info("context: stoping....");
  _stopping_begin = true;

#ifdef YAAF_NETWORK_ENABLED
  logger_info("context: network stopping");
  for (auto l : _network_connections) {
    l.second->disconnect();
    l.second->wait_stoping();
  }

  for (auto l : _network_listeners) {
    l.second->stop();
    l.second->wait_stoping();
  }

  _stopping_begin = true;
  for (auto &&t : _net_threads) {
    t.join();
  }
#endif

  _thread_manager->stop();
  _thread_manager = nullptr;

  std::lock_guard<std::shared_mutex> lg(_locker);
#ifdef YAAF_NETWORK_ENABLED
  _network_listeners.clear();
#endif

  logger_info("context: threads pools stopped.");

  for (auto kv : _actors) {
    logger_info("context: ", kv.second->name, " stopped.");
    kv.second->actor->on_stop();
    kv.second->usrcont = nullptr;
  }
  logger_info("context: clear buffer.");
  _actors.clear();
  _mboxes.clear();
  _id_by_name.clear();
  logger_info("context: stoped");
}

task_result_ptr context::user_post(const std::function<void()> &f,
                                   utils::async::CONTINUATION_STRATEGY strategy) {
  task t = [f, strategy](const thread_info &tinfo) {
    TKIND_CHECK(tinfo.kind, USER);
    f();
    return strategy;
  };

  return _thread_manager->post(USER, wrap_task(t));
}

task_result_ptr context::sys_post(const std::function<void()> &f,
                                  utils::async::CONTINUATION_STRATEGY strategy) {
  task t = [f, strategy](const thread_info &tinfo) {
    TKIND_CHECK(tinfo.kind, SYSTEM);
    f();
    return strategy;
  };

  return _thread_manager->post(SYSTEM, wrap_task(t));
}

std::string context::name() const {
  return _name;
}

actor_address context::add_actor(const std::string &actor_name, const actor_ptr a) {
  return add_actor(actor_name, actor_address{}, a);
}

actor_address context::add_actor(const std::string &actor_name,
                                 const actor_address &parent, const actor_ptr a) {
  auto new_id = id_t(_next_actor_id++);

  logger_info("context: add actor #", actor_name);
  auto self = shared_from_this();

  auto d = std::make_shared<inner::description>();
  auto ucname = name() + "/#usercontext#" + std::to_string(new_id.value);

  auto settings = actor_settings::defsettings();
  std::string parent_name = "";

  actor_address cur_parent = parent.empty() ? _usr_root : parent;
  std::shared_ptr<inner::description> parent_description = nullptr;

  if (!cur_parent.empty()) {
    std::shared_lock<std::shared_mutex> lg(_locker);
    parent_description = _actors[cur_parent.get_id()];
    d->parent = cur_parent.get_id();
    settings = parent_description->settings;
    parent_name = parent_description->name;
  } else {
    parent_name = "";
  }

  d->name = parent_name + "/" + actor_name;
  actor_address result{new_id, d->name};
  d->usrcont = std::make_shared<user_context>(self, result, ucname);
  a->set_context(d->usrcont);
  d->actor = a;
  d->address = result;
  d->settings = a->on_init(settings);

  a->set_self_addr(result);

  {
    std::lock_guard<std::shared_mutex> lg(_locker);
    if (parent_description != nullptr) {
      parent_description->children.insert(new_id);
    }

    _actors[new_id] = d;
    _id_by_name[d->name] = new_id;
    _mboxes[new_id] = std::make_shared<mailbox>();
  }

  user_post([this, a]() { a->on_start(); });

  return result;
}

void context::create_exchange(const actor_address &owner, const std::string &name) {
  std::lock_guard<std::shared_mutex> lg(_exchange_locker);
  create_exchange_unsafe(owner, name);
}

void context::create_exchange_unsafe(const actor_address &owner,
                                     const std::string &name) {
  logger_info("context: add exchange #", name);
  if (_exchanges.find(name) != _exchanges.end()) {
    logger_info("context: add exchange #", name, " - already created");
    return;
  } else {
    inner::exchange_t e;
    e.owner = owner;
    e.pub = std::make_shared<mailbox>();

    _exchanges[name] = e;
  }
}

void context::subscribe_to_exchange(const actor_address &target,
                                    const std::string &name) {
  std::unique_lock<std::shared_mutex> elg(_exchange_locker, std::defer_lock);
  std::unique_lock<std::shared_mutex> lg(_locker, std::defer_lock);
  std::lock(elg, lg);

  logger_info("context: subscribe to exchange ", target, " <= ", name);
  auto eit = _exchanges.find(name);
  if (eit == _exchanges.end()) {
    create_exchange_unsafe(target, name);
    eit = _exchanges.find(name);
  }

  auto ait = _actors.find(target.get_id());
  if (ait != _actors.end()) {
    eit->second.subscribes.push_back(target.get_id());
  }
}

void context::publish_to_exchange(const std::string &exchange, const envelope &e) {
  publish_to_exchange(exchange, std::move(e));
}

void context::publish_to_exchange(const std::string &exchange, const envelope &&e) {
  std::shared_lock<std::shared_mutex> lg(_exchange_locker);
  logger_info("context: publish to '", exchange, "'");
  auto eit = _exchanges.find(exchange);
  if (eit != _exchanges.end()) {
    eit->second.pub->push(e);
  } else {
    logger_info("context: exchange not found - ", exchange);
  }
};

bool context::exchange_exists(const std::string &name) const {
  std::shared_lock<std::shared_mutex> lg(_exchange_locker);
  return _exchanges.find(name) != _exchanges.end();
}

actor_weak context::get_actor(const actor_address &addr) const {
  std::shared_lock<std::shared_mutex> lg(_locker);
  logger_info("context: get actor #", addr);
  auto it = _actors.find(addr.get_id());
  actor_weak result;
  if (it != _actors.end()) { // actor may be stopped
    result = it->second->actor;
  }
  return result;
}

actor_weak context::get_actor(const std::string &name) const {
  std::shared_lock<std::shared_mutex> lg(_locker);
  logger_info("context: get actor by name '", name, "'");
  actor_weak result;
  auto id_it = _id_by_name.find(name);
  if (id_it != _id_by_name.end()) { // actor may be stopped
    auto it = _actors.find(id_it->second);
    if (it != _actors.end()) {
      result = it->second->actor;
    }
  }
  return result;
}

actor_address context::get_address(const std::string &name) const {
  std::shared_lock<std::shared_mutex> lg(_locker);
  logger_info("context: get actor address by name '", name, "'");
  actor_address result;
  auto id_it = _id_by_name.find(name);
  if (id_it != _id_by_name.end()) { // actor may be stopped
    auto it = _actors.find(id_it->second);
    if (it != _actors.end()) {
      result = it->second->address;
    }
  }
  return result;
}

void context::send_envelope(const actor_address &target, const envelope &e) {
  std::shared_lock<std::shared_mutex> lg(_locker);
  ENSURE(target.get_pathname() != "null");
  ENSURE(e.sender.get_pathname() != "null");
  logger_info("context: send to: ", target);
  auto it = _mboxes.find(target.get_id());
  if (it != _mboxes.end()) { // actor may be stopped
    it->second->push(e);
  }
}

void context::send_envelope(const actor_address &target, const envelope &&e) {
  std::shared_lock<std::shared_mutex> lg(_locker);
  ENSURE(target.get_pathname() != "null");
  ENSURE(e.sender.get_pathname() != "null");
  logger_info("context: send to: ", target);
  auto it = _mboxes.find(target.get_id());
  if (it != _mboxes.end()) { // actor may be stopped
    it->second->push(std::move(e));
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

void context::apply_actor_to_mailbox(
    const std::shared_ptr<inner::description> target_actor_description, actor_ptr parent,
    std::shared_ptr<mailbox> mb) {
  logger_info("context: apply ", target_actor_description->name);
  try {
    target_actor_description->actor->apply(*mb);
    if (parent != nullptr) {
      parent->on_child_status(target_actor_description->address,
                              actor_status_kinds::NORMAL);
    }
  } catch (std::exception &ex) {
    logger_warn(ex.what());

    if (parent != nullptr) {
      auto action = parent->on_child_error(target_actor_description->address);
      if (action == actor_action_when_error::ESCALATE) {
        std::shared_lock<std::shared_mutex> lg(_locker);
        auto descr = target_actor_description;
        while (action == actor_action_when_error::ESCALATE) {
          auto parent_it = _actors.find(descr->parent);
          action = parent_it->second->actor->on_child_error(descr->address);
          descr = parent_it->second;
        }
      }
      on_actor_error(action, target_actor_description, parent);
    } else {
      this->stop_actor_impl_safety(target_actor_description->address,
                                   actor_stopping_reason::EXCEPT);
    }
  }
}

void context::on_actor_error(
    actor_action_when_error action,
    const std::shared_ptr<inner::description> target_actor_description,
    actor_ptr parent) {

  auto &target_addr = target_actor_description->address;

  switch (action) {
  case actor_action_when_error::REINIT:
    logger_info("context: on_actor_error ", target_addr, " action: REINIT");
    stop_actor_impl_safety(target_addr, actor_stopping_reason::EXCEPT);
    target_actor_description->actor->on_start();
    break;
  case actor_action_when_error::STOP:
    logger_info("context: on_actor_error ", target_addr, " action: STOP");
    stop_actor_impl_safety(target_addr, actor_stopping_reason::EXCEPT);
    break;
  case actor_action_when_error::ESCALATE:
    logger_info("context: on_actor_error ", target_addr, " action: ESCALATE");
    break;
  case actor_action_when_error::RESUME:
    logger_info("context: on_actor_error ", target_addr, " action: RESUME");
    parent->on_child_status(target_addr, actor_status_kinds::WITH_ERROR);
    break;
  }
}

void context::mailbox_worker() {
  if (_exchange_locker.try_lock_shared()) {
    if (_locker.try_lock()) {
      for (auto kv : _exchanges) {
        yaaf::envelope e;
        while (kv.second.pub->try_pop(e)) {
          for (auto id : kv.second.subscribes) {
            auto mb = _mboxes.find(id);
            if (mb != _mboxes.end()) {
              yaaf::envelope cp;
              cp.payload = e.payload;
              cp.sender = e.sender;
              mb->second->push(e);
            }
          }
        }
      }
      _locker.unlock();
    }
    _exchange_locker.unlock_shared();
  }

  if (!_locker.try_lock_shared()) {
    return;
  }
  std::unordered_set<id_t> to_remove;

  for (auto kv : _mboxes) {
    auto mb = kv.second;
    if (!mb->empty()) {

      auto it = _actors.find(kv.first);
      if (it == _actors.end()) {
        to_remove.insert(kv.first);
        continue;
      }

      auto target_actor_description = it->second;

      if (target_actor_description->actor->try_lock()) {
        if (mb->empty()) {
          target_actor_description->actor->reset_busy();
        } else {
          actor_ptr parent = nullptr;

          if (!target_actor_description->parent.empty()) {
            parent = _actors[target_actor_description->parent]->actor;
          }
          logger_info("context: push to run queue #", target_actor_description->address);
          user_post([this, target_actor_description, parent, mb]() {
            this->apply_actor_to_mailbox(target_actor_description, parent, mb);
          });
        }
      }
    }
  }
  if (_exchange_locker.try_lock()) {
    std::list<std::string> ex_to_remove;
    for (auto &kv : _exchanges) {
      auto owner_exists = _actors.find(kv.second.owner.get_id()) != _actors.end();
      auto subsribers = std::any_of(
          kv.second.subscribes.begin(), kv.second.subscribes.end(),
          [this](const id_t id) { return _actors.find(id) != _actors.end(); });

      if (!owner_exists && !subsribers) {
        ex_to_remove.push_back(kv.first);
      }
    }

    for (auto &n : ex_to_remove) {
      _exchanges.erase(n);
    }

    _exchange_locker.unlock();
  }
  _locker.unlock_shared();

  if (!to_remove.empty()) {
    if (_locker.try_lock()) {
      for (auto v : to_remove) {
        _mboxes.erase(v);
      }
      _locker.unlock();
    }
  }

  // TODO need a sleeping?
}

#ifndef YAAF_NETWORK_ENABLED
void context::network_init() {}
#endif