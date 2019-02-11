#include <libyaaf/abstract_context.h>
#include <libyaaf/actor.h>
#include <libyaaf/envelope.h>
#include <libyaaf/utils/logger.h>

using namespace yaaf;
using namespace yaaf::utils::logging;


abstract_context::~abstract_context() {
  logger_info("~abstract_context");
}
