#pragma once

#include <libnmq/actor.h>
#include <libnmq/exports.h>
#include <libnmq/types.h>

#include <libnmq/utils/async/thread_manager.h>

#include <memory>
#include <shared_mutex>
#include <unordered_set>

namespace nmq {
class abstract_context;

namespace inner {

struct description {
  actor_ptr actor;
  actor_settings settings;
  std::shared_ptr<abstract_context> usrcont;
  id_t parent;
  std::unordered_set<id_t> children;
};
} // namespace inner

class abstract_context {
public:
  EXPORT virtual ~abstract_context();

  template <class ACTOR_T, class... ARGS> actor_address make_actor(ARGS &&... a) {
    auto new_a = std::make_shared<ACTOR_T>(std::forward<ARGS>(a)...);
    return add_actor(new_a);
  }

  EXPORT actor_address add_actor(const actor_for_delegate::delegate_t f);

  inline actor_ptr get_actor(const actor_address &a) { return get_actor(a.get_id()); }

  virtual actor_address add_actor(const actor_ptr a) = 0;

  template <class T> void send(const actor_address &target, T &&t) {
    envelope e;
    e.payload = std::forward<T>(t);
    send(target, e);
  }

  void send(const actor_address &target, envelope e) { send_envelope(target, e); }

  virtual void send_envelope(const actor_address &target, envelope msg) = 0;

  virtual void stop_actor(const actor_address &addr) = 0;
  virtual actor_ptr get_actor(id_t id) = 0;
};

class context : public abstract_context, public std::enable_shared_from_this<context> {
public:
  using abstract_context::add_actor;
  using abstract_context::get_actor;
  using abstract_context::make_actor;
  using abstract_context::send;

  struct params_t {
    EXPORT static params_t defparams();

    size_t user_threads;
    size_t sys_threads;
  };

  EXPORT static std::shared_ptr<context> make_context();
  EXPORT static std::shared_ptr<context> make_context(const params_t &params);

  EXPORT context(const params_t &p);
  EXPORT ~context();
  void start();

  EXPORT void send_envelope(const actor_address &target, envelope msg) override;
  EXPORT actor_address add_actor(const actor_ptr a) override;
  EXPORT actor_address add_actor(const actor_address &parent, const actor_ptr a);
  EXPORT void stop_actor(const actor_address &addr) override;
  EXPORT actor_ptr get_actor(id_t id) override;

private:
  void mailbox_worker();
  void stop_actor_impl_safety(const actor_address &addr, actor_stopping_reason reason);
  void stop_actor_impl(const actor_address &addr, actor_stopping_reason reason);
  
private:
  params_t _params;

  std::unique_ptr<utils::async::thread_manager> _thread_manager;

  std::shared_mutex _locker;
  std::atomic_uint64_t _next_actor_id{1};

  std::unordered_map<id_t, std::shared_ptr<inner::description>> _actors;
  std::unordered_map<id_t, std::shared_ptr<mailbox>> _mboxes;
};

} // namespace nmq
