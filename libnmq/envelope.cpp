#include <libnmq/context.h>
#include <libnmq/envelope.h>

using namespace nmq;

void actor_address::stop() {
  if (auto ptr = _ctx.lock()) {
    ptr->stop_actor(*this);
  }
}
