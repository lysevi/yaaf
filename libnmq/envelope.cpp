#include <libnmq/envelope.h>
#include <libnmq/context.h>

using namespace nmq;

void actor_address::stop() {
  _ctx->stop_actor(*this);
}
