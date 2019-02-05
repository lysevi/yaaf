#include <libnmq/actor.h>
#include <libnmq/context.h>

using namespace nmq;

base_actor::~base_actor() {
  
}

void base_actor::on_start() {}

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

void base_actor::set_self_addr(actor_address sa) {
  _sa = sa;
}

actor_for_delegate::actor_for_delegate(actor_for_delegate::delegate_t callback)
    : _handle(callback) {}

void actor_for_delegate::action_handle(envelope &e) {
  _handle(e);
}
