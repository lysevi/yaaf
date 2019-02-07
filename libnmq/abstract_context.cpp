#include <libnmq/abstract_context.h>
#include <libnmq/actor.h>
#include <libnmq/envelope.h>
#include <libnmq/utils/logger.h>

using namespace nmq;
using namespace nmq::utils::logging;

void abstract_context::send(const actor_address &target, envelope e) {
  send_envelope(target, e);
}

actor_ptr abstract_context::get_actor(const actor_address &a) {
  return get_actor(a.get_id());
}

abstract_context::~abstract_context() {
  logger_info("~abstract_context");
}
