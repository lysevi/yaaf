#pragma once

#include <libnmq/actor_settings.h>
#include <libnmq/exports.h>
#include <libnmq/mailbox.h>
#include <libnmq/utils/async/locker.h>
#include <functional>
#include <memory>

namespace nmq {

class abstract_context;

enum class actor_status_kinds { NORMAL, WITH_ERROR, STOPED };
enum class actor_stopping_reason { MANUAL, EXCEPT };

class base_actor : public std::enable_shared_from_this<base_actor> {
public:
  struct status_t {
    actor_status_kinds kind = actor_status_kinds::NORMAL;
    std::string msg="";
  };

  base_actor() {
    _busy = false;
    _status.kind = actor_status_kinds::NORMAL;
  }

  EXPORT virtual ~base_actor();
  EXPORT virtual actor_settings on_init(const actor_settings &base_settings) ;
  EXPORT virtual void on_start() ;
  EXPORT virtual void on_stop() ;
  EXPORT virtual void on_child_status(const actor_address &addr,
                                      actor_status_kinds k) ;
  EXPORT virtual void on_child_stopped(const actor_address &addr,
                                       const actor_stopping_reason reason) ;
  EXPORT virtual void apply(mailbox &mbox);

  virtual void action_handle(const envelope &e) = 0;

  EXPORT bool try_lock();

  bool busy() const { return _busy.load(); }
  status_t status() const { return _status; }

  void reset_busy() { _busy.store(false); }

  EXPORT actor_address self_addr();
  EXPORT void set_self_addr(const actor_address &sa);

  std::shared_ptr<abstract_context> get_context() const { return _ctx.lock(); }
  void set_context(std::weak_ptr<abstract_context> ctx_) { _ctx = ctx_; }

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

  actor_address _sa;
  std::weak_ptr<abstract_context> _ctx;
};

class actor_for_delegate : public base_actor {
public:
  using delegate_t = std::function<void(const envelope &)>;

  actor_for_delegate(const actor_for_delegate &a) = delete;
  actor_for_delegate() = delete;
  actor_for_delegate(actor_for_delegate &&a) = delete;

  EXPORT actor_for_delegate(delegate_t callback);
  ~actor_for_delegate() {}

  EXPORT void action_handle(const envelope &e) override;

private:
  delegate_t _handle;
};

} // namespace nmq