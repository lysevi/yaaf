#pragma once

#include <libnmq/network/message.h>
#include <type_traits>

namespace nmq {
namespace network {

enum class messagekinds : network::message::kind_t {
  OK,
  LOGIN,
  LOGIN_CONFIRM,
  LOGIN_FAILED,
  MSG,
};
}
} // namespace nmq
