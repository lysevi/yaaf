#include <libnmq/actor.h>
#include <libnmq/context.h>
#include <libnmq/mailbox.h>

using namespace nmq;

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
actor::actor(context *ctx, actor::delegate_t callback)
    : base_actor(ctx), _handle(callback) {}

void actor::apply(mailbox &mbox) {
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
      nmq::actor_weak aweak(self);
      _handle(aweak, el);
    }
    update_status(actor_status_kinds::NORMAL);
    reset_busy();
  } catch (std::exception &ex) {
    update_status(actor_status_kinds::WITH_ERROR, ex.what());
    reset_busy();
    throw;
  }
}
