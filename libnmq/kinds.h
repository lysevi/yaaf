#pragma once

#include <libnmq/network_message.h>
#include <type_traits>

namespace nmq {
enum class MessageKinds : NetworkMessage::message_kind {
  OK,
  LOGIN,
  LOGIN_CONFIRM
};
} // namespace nmq
