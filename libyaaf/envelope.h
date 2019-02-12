#pragma once

#include <libyaaf/actor_address.h>
#include <libyaaf/exports.h>
#include <libyaaf/payload.h>
#include <libyaaf/types.h>

namespace yaaf {

struct envelope {
  payload_t payload;
  actor_address sender;
};
} // namespace yaaf