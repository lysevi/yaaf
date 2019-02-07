#pragma once

#include <libnmq/envelope.h>
#include <libnmq/exports.h>
#include <functional>

namespace nmq {
class actor_address;
struct envelope;
class base_actor;
class actor_for_delegate;

using actor_ptr = std::shared_ptr<base_actor>;

class abstract_context {
public:
  EXPORT virtual ~abstract_context();

  template <class ACTOR_T, class... ARGS> actor_address make_actor(ARGS &&... a) {
    auto new_a = std::make_shared<ACTOR_T>(std::forward<ARGS>(a)...);
    return add_actor(new_a);
  }

  template <class T> void send(const actor_address &target, T &&t) {
    envelope e;
    e.payload = std::forward<T>(t);
    send(target, e);
  }

  EXPORT actor_ptr get_actor(const actor_address &a);
  EXPORT void send(const actor_address &target, envelope e);

  virtual actor_address add_actor(const actor_ptr a) = 0;
  virtual void send_envelope(const actor_address &target, envelope msg) = 0;

  virtual void stop_actor(const actor_address &addr) = 0;
  virtual actor_ptr get_actor(id_t id) = 0;
};
} // namespace nmq