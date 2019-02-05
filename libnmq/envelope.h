#pragma once

#include <libnmq/actor.h>
#include <boost/any.hpp>

namespace nmq {
struct envelope {
  boost::any payload;

  actor_weak sender;
};
} // namespace nmq
