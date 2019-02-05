#include <libnmq/actor.h>
#include <libnmq/mailbox.h>

using namespace nmq;

void actor::apply(mailbox &mbox) {
  if (mbox.empty()) {
    return;
  }
  bool expect = _busy.load();
  if (expect) {
    return;
  }
  if (!_busy.compare_exchange_strong(expect, true)) {
    return;
  }
  auto self = shared_from_this();
  try {
    envelope el;
    if (mbox.try_pop(el)) {
      nmq::actor_weak aweak(self);
      _handle(aweak, el);
    }
    _status.kind = actor_status_kinds::NORMAL;
    _status.msg.clear();

  } catch (std::exception &ex) {
    _busy.store(false);
    _status.kind = actor_status_kinds::WITH_ERROR;
    _status.msg = ex.what();
    throw;
  }
  _busy.store(false);
}
