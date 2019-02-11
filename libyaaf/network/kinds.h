#pragma once

#include <libyaaf/network/message.h>
#include <type_traits>

namespace yaaf {
namespace network {

enum class messagekinds : network::message::kind_t {
  OK,
  LOGIN,
  LOGIN_CONFIRM,
  LOGIN_FAILED,
  MSG,
};
}
} // namespace yaaf
