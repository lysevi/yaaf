#pragma once

#include <libnmq/actor.h>
#include <libnmq/envelope.h>
#include <libnmq/exports.h>
#include <libnmq/mailbox.h>
#include <libnmq/types.h>

#include <libnmq/utils/async/thread_manager.h>

#include <memory>
#include <shared_mutex>

namespace nmq {
class context;

class actor_address {
public:
  actor_address(id_t id_, context *ctx_) : _id(id_), _ctx(ctx_) {}
  ~actor_address() {}

  id_t get_id() const { return _id; }

  template <class T> void send(T &&t) {
    envelope ev;
    ev.payload = t;
    _ctx->send(*this, ev);
  }

  EXPORT void stop();

private:
  id_t _id;
  context *_ctx;
};

class context : public std::enable_shared_from_this<context> {
public:
  struct params_t {
    EXPORT static params_t defparams();

    size_t user_threads;
    size_t sys_threads;
  };

  EXPORT context(params_t params);
  EXPORT ~context();
  EXPORT actor_address add_actor(actor::delegate_t f);
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
