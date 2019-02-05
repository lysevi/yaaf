#pragma once

#include <libnmq/actor.h>
#include <libnmq/exports.h>
#include <libnmq/types.h>

#include <libnmq/utils/async/thread_manager.h>

#include <memory>
#include <shared_mutex>

namespace nmq {

class context : public std::enable_shared_from_this<context> {
public:
  struct params_t {
    EXPORT static params_t defparams();

    size_t user_threads;
    size_t sys_threads;
  };

  EXPORT context(params_t params);
  EXPORT ~context();
  EXPORT actor_address add_actor(actor_for_delegate::delegate_t f);
  EXPORT actor_address add_actor(actor_ptr a);
  EXPORT void send(actor_address &addr, envelope msg);
  EXPORT void stop_actor(actor_address &addr);

private:
  void mailbox_worker();

private:
  params_t _params;

  std::unique_ptr<utils::async::thread_manager> _thread_manager;

  std::shared_mutex _locker;
  std::uint64_t _next_actor_id{0};

  std::unordered_map<id_t, actor_ptr> _actors;
  std::unordered_map<id_t, std::shared_ptr<mailbox>> _mboxes;
};

using context_ptr = std::shared_ptr<context>;
} // namespace nmq
