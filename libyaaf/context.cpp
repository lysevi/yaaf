#include <libyaaf/context.h>
#include <libyaaf/network/queries.h>
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

class user_context : public abstract_context {
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

  std::string name() const override { return _name; }

private:
  std::weak_ptr<context> _ctx;
  actor_address _addr;
  std::string _name;
};

class usr_actor : public base_actor {
public:
  void action_handle(const envelope &e) { UNUSED(e); }
};

class sys_actor : public base_actor {
public:
  void action_handle(const envelope &e) { UNUSED(e); }
};

class root_actor : public base_actor {
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
    l->disconnect();
    l->wait_stoping();
  }

  for (auto l : _network_listeners) {
    l->stop();
    l->wait_stoping();
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

#ifdef YAAF_NETWORK_ENABLED

namespace yaaf {
namespace serialization {
template <> struct object_packer<network_actor_message> {
  using Scheme = yaaf::serialization::binary_io<std::string, std::vector<unsigned char>>;

  static size_t capacity(const network_actor_message &t) {
    return Scheme::capacity(t.name, t.data);
  }

  template <class Iterator> static void pack(Iterator it, const network_actor_message t) {
    Scheme::write(it, t.name, t.data);
  }

  template <class Iterator> static network_actor_message unpack(Iterator ii) {
    network_actor_message t{};
    Scheme::read(ii, t.name, t.data);
    return t;
  }
};

template <> struct object_packer<listener_message> {
  using Scheme = yaaf::serialization::binary_io<uint64_t>;

  static size_t capacity(const listener_message &t) {
    return Scheme::capacity(t.sender) +
           object_packer<network_actor_message>::capacity(t.msg);
  }

  template <class Iterator> static void pack(Iterator it, const listener_message t) {
    Scheme::write(it, t.sender);
    object_packer<network_actor_message>::pack(it + Scheme::capacity(t.sender), t);
  }

  template <class Iterator> static listener_message unpack(Iterator ii) {
    listener_message t{};
    Scheme::read(ii, t.sender);
    object_packer<network_actor_message>::pack(it + Scheme::capacity(t.sender), t.msg);
    return t;
  }
};
} // namespace serialization
} // namespace yaaf

namespace {
class network_actor : public base_actor {
public:
  void action_handle(const envelope &e) { UNUSED(e); }
};

class network_lst_actor : public base_actor,
                          public yaaf::network::abstract_listener_consumer {
public:
  void action_handle(const envelope &e) {
    auto lm = e.payload.cast<listener_message>();
    yaaf::network::queries::packed_message<yaaf::network_actor_message> pm(lm.msg.name,
                                                                           lm.msg);
    auto msg_ptr = pm.get_message();
    send_to(lm.sender, msg_ptr);
  }

  bool on_new_connection(yaaf::network::listener_client_ptr c) override { return true; }

  void on_network_error(yaaf::network::listener_client_ptr i,
                        const network::message_ptr & /*d*/,
                        const boost::system::error_code & /*err*/) override {}

  void on_new_message(yaaf::network::listener_client_ptr i, network::message_ptr &&d,
                      bool & /*cancel*/) override {
    if (d->get_header()->kind == (network::message::kind_t)network::messagekinds::MSG) {
      network::queries::packed_message<network_actor_message> nm(d);
      auto ctx = get_context();
      if (ctx != nullptr) {
        auto actor_ = ctx->get_actor(nm.actorname);
        if (auto sp = actor_.lock()) {
          listener_message lm;
          lm.msg = nm.msg;
          lm.sender = i->get_id().value;
          ctx->send(sp->self_addr(), lm);
        } else {
          logger_fatal("context: listener - cannot get actor ", nm.actorname);
        }
      }
    }
  }

  void on_disconnect(const yaaf::network::listener_client_ptr & /*i*/) override {}
};

class network_con_actor : public base_actor,
                          public yaaf::network::abstract_connection_consumer {
public:
  network_con_actor(std::shared_ptr<yaaf::network::connection> con_) { _con = con_; }

  void action_handle(const envelope &e) {
    network_actor_message nm = e.payload.cast<network_actor_message>();
    yaaf::network::queries::packed_message<yaaf::network_actor_message> pm(nm.name, nm);

    _con->send_async(pm.get_message());
  }

  void on_connect() override{};
  void on_new_message(yaaf::network::message_ptr &&d, bool &quet) override {
    UNUSED(quet);
    if (d->get_header()->kind == (network::message::kind_t)network::messagekinds::MSG) {
      network::queries::packed_message<network_actor_message> nm(d);
      auto ctx = get_context();
      if (ctx != nullptr) {
        auto target_actor = ctx->get_actor(nm.actorname);
        if (auto sp = target_actor.lock()) {
          ctx->send(sp->self_addr(), nm.msg);
        } else {
          logger_fatal("context: connection - cannot get actor ", nm.actorname);
        }
      }
    }
  }
  void on_network_error(const yaaf::network::message_ptr &,
                        const boost::system::error_code &err) override {
    bool isError = err == boost::asio::error::operation_aborted ||
                   err == boost::asio::error::connection_reset ||
                   err == boost::asio::error::eof;
    if (isError && !is_stoped()) {
      auto msg = err.message();
      yaaf::utils::logging::logger_fatal(msg);
    }
  }

private:
  std::shared_ptr<yaaf::network::connection> _con;
};

} // namespace
void context::network_init() {
  logger_info("context: network init...");

  _net_root = this->add_actor("net", _root, std::make_shared<network_actor>());

  if (_params.listeners_params.empty() && _params.connection_params.empty()) {
    logger_info("context: network params is empty.");
    return;
  }

  for (int i = 0; i < _params.network_threads; ++i) {
    _net_threads.emplace_back([this]() {
      while (!this->is_stopping_begin()) {
        this->_net_service.poll_one();
      }
    });
  }

  for (auto lp : _params.listeners_params) {
    logger_info("context: start listener on ", lp.port);
    auto l = std::make_shared<network::listener>(&this->_net_service, lp);
    auto saptr = std::make_shared<network_lst_actor>();
    auto lactor = this->add_actor("listen_" + std::to_string(lp.port), _net_root, saptr);

    l->add_consumer(saptr.get());
    l->start();
    l->wait_starting();
    _network_listeners.emplace_back(l);
  }

  for (auto lp : _params.connection_params) {
    logger_info("context: connecting to ", lp.host, ':', lp.port);
    auto l = std::make_shared<network::connection>(&this->_net_service, lp);
    auto actor_name = lp.host + ':' + std::to_string(lp.port);
    auto saptr = std::make_shared<network_con_actor>(l);
    auto lactor = this->add_actor(actor_name, _net_root, saptr);

    l->add_consumer(saptr.get());
    l->start_async_connection();
    l->wait_starting();
    _network_connections.emplace_back(l);
  }
}
#else
void context::network_init() {}
#endif