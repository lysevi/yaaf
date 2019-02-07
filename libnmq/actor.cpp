#include <libnmq/abstract_context.h>
#include <libnmq/actor.h>
#include <libnmq/utils/utils.h>

using namespace nmq;

base_actor::~base_actor() {}

actor_settings base_actor::on_init(const actor_settings &base_settings) {
  return base_settings;
}

void base_actor::on_start() {}
void base_actor::on_stop() {
  update_status(actor_status_kinds::STOPED);
}

void base_actor::on_child_status(const actor_address &addr, actor_status_kinds k) {
  UNUSED(addr);
  UNUSED(k);
}

void base_actor::on_child_stopped(const actor_address &addr,
                                  const actor_stopping_reason reason) {
  UNUSED(addr);
  UNUSED(reason);
}

void base_actor::apply(mailbox &mbox) {
  if (mbox.empty()) {
    update_status(actor_status_kinds::NORMAL);
    reset_busy();
    return;
  }

  ENSURE(busy());

  auto self = shared_from_this();
  try {
    envelope el;
    if (mbox.try_pop(el)) {
      action_handle(el);
    }
    update_status(actor_status_kinds::NORMAL);
    reset_busy();
  } catch (std::exception &ex) {
    update_status(actor_status_kinds::WITH_ERROR, ex.what());
    reset_busy();
    throw;
  }
}

bool base_actor::try_lock() {
  bool expect = _busy.load();
  if (expect) {
    return false;
  }
  if (!_busy.compare_exchange_strong(expect, true)) {
    return false;
  }
  return true;
}

actor_address base_actor::self_addr() {
  return _sa;
}

void base_actor::set_self_addr(const actor_address &sa) {
  _sa = sa;
}

actor_for_delegate::actor_for_delegate(actor_for_delegate::delegate_t callback)
    :  _handle(callback) {}

void actor_for_delegate::action_handle(const envelope &e) {
  _handle(e);
}
