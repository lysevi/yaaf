#include <libnmq/context.h>
#include <libnmq/envelope.h>

using namespace nmq;

void actor_address::stop() {
  _ctx->stop_actor(*this);
}
