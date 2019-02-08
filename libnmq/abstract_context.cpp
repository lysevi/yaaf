#include <libnmq/abstract_context.h>
#include <libnmq/actor.h>
#include <libnmq/envelope.h>
#include <libnmq/utils/logger.h>

using namespace nmq;
using namespace nmq::utils::logging;


abstract_context::~abstract_context() {
  logger_info("~abstract_context");
}
