#pragma once

#include <libnmq/network/message.h>
#include <type_traits>

namespace nmq {
enum class MessageKinds: network::Message::message_kind_t {
  OK,
  LOGIN,
  LOGIN_CONFIRM
};
} // namespace nmq
