#pragma once

#include <libnmq/actor.h>
#include <libnmq/exports.h>
#include <libnmq/types.h>

#include <libnmq/utils/async/thread_manager.h>

#include <memory>
#include <shared_mutex>

namespace nmq {
namespace inner {

struct description {
  actor_ptr actor;
  actor_settings settings;
};
} // namespace inner

class context : public std::enable_shared_from_this<context> {
public:
  struct params_t {
    EXPORT static params_t defparams();

    size_t user_threads;
    size_t sys_threads;
  };

  EXPORT static std::shared_ptr<context> make_context();
  EXPORT static std::shared_ptr<context> make_context(const params_t &params);

  EXPORT context(const params_t &p);
  EXPORT ~context();

  EXPORT actor_address add_actor(const actor_for_delegate::delegate_t f);
  EXPORT actor_address add_actor(const actor_ptr a);

  EXPORT void send(const actor_address &addr, const envelope &msg);
  EXPORT void stop_actor(const actor_address &addr);

  inline actor_ptr get_actor(const actor_address &a) { return get_actor(a.get_id()); }
  EXPORT actor_ptr get_actor(id_t id);

  template <class ACTOR_T, class... ARGS> actor_address make_actor(ARGS &&... a) {
    auto new_a = std::make_shared<ACTOR_T>(std::forward<ARGS>(a)...);
    return add_actor(new_a);
  }

private:
  void mailbox_worker();

private:
  params_t _params;

  std::unique_ptr<utils::async::thread_manager> _thread_manager;

  std::shared_mutex _locker;
  std::uint64_t _next_actor_id{0};

  std::unordered_map<id_t, inner::description> _actors;
  std::unordered_map<id_t, std::shared_ptr<mailbox>> _mboxes;
};

using context_ptr = std::shared_ptr<context>;
} // namespace nmq
