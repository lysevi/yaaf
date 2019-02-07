#include <libnmq/actor_settings.h>

using namespace nmq;

actor_settings actor_settings::defsettings() {
  actor_settings result;
  result.stop_on_any_error = false;
  return result;
}