#pragma once

#include <libnmq/exports.h>

namespace nmq {

class actor_settings {
public:
  static EXPORT actor_settings defsettings();

  bool stop_on_any_error;
};

}; // namespace nmq