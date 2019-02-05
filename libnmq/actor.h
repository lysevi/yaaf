#pragma once

#include <libnmq/exports.h>
#include <libnmq/utils/async/locker.h>
#include <functional>
#include <memory>

namespace nmq {

struct envelope;
class mailbox;
class base_actor;
class actor_for_delegate;
class context;

using actor_ptr = std::shared_ptr<base_actor>;
using actor_weak = std::weak_ptr<base_actor>;

enum class actor_status_kinds { NORMAL, WITH_ERROR };

class base_actor : public std::enable_shared_from_this<base_actor> {
public:
  struct status_t {
    actor_status_kinds kind;
    std::string msg;
  };

  base_actor(context *ctx) : _ctx(ctx) {
    _busy = false;
    _status.kind = actor_status_kinds::NORMAL;
  }

  EXPORT virtual void on_start();
  EXPORT virtual void apply(mailbox &mbox);
  virtual void action_handle(envelope&e)=0;

  EXPORT bool try_lock();
  bool busy() const { return _busy.load(); }
  status_t status() const { return _status; }

  context *ctx() { return _ctx; }

  void reset_busy() { _busy.store(false); }

protected:
  void update_status(actor_status_kinds kind) {
    _status.kind = kind;
    _status.msg.clear();
  }

  void update_status(actor_status_kinds kind, const std::string &msg) {
    _status.kind = kind;
    _status.msg = msg;
  }

private:
  mutable std::atomic_bool _busy;
  status_t _status;

  context *_ctx;
};

class actor_for_delegate : public base_actor {
public:
  using delegate_t = std::function<void(const envelope &)>;

  actor_for_delegate(const actor_for_delegate &a) = delete;
  actor_for_delegate() = delete;
  actor_for_delegate(actor_for_delegate &&a) = delete;

  EXPORT actor_for_delegate(context *ctx, delegate_t callback);
  ~actor_for_delegate() {}

  EXPORT void action_handle(envelope &e) override;

private:
  delegate_t _handle;
};

} // namespace nmq