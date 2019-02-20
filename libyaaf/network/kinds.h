#pragma once

#include <libdialler/message.h>
#include <type_traits>

namespace yaaf {
namespace network {

enum class messagekinds : dialler::message::kind_t {
  OK,
  LOGIN,
  LOGIN_CONFIRM,
  LOGIN_FAILED,
  MSG,
};
}
} // namespace yaaf
