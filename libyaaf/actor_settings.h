#pragma once

#include <libyaaf/exports.h>

namespace yaaf {

class actor_settings {
public:
  static EXPORT actor_settings defsettings();

  bool stop_on_any_error;
};

}; // namespace yaaf
