#pragma once

#include <libyaaf/envelope.h>
#include <libyaaf/exports.h>
#include <string>

namespace yaaf {
class base_actor;

using actor_ptr = std::shared_ptr<base_actor>;
using actor_weak = std::weak_ptr<base_actor>;

class abstract_context {
public:
  EXPORT virtual ~abstract_context();

  template <class ACTOR_T, class... ARGS>
  actor_address make_actor(const std::string &actor_name, ARGS &&... a) {
    auto new_a = std::make_shared<ACTOR_T>(std::forward<ARGS>(a)...);
    return add_actor(actor_name, new_a);
  }

  template <class T> void send(const actor_address &target, T &&t) {
    envelope e;
    e.payload = std::forward<T>(t);
    send_envelope(target, e);
  }

  template <class T> void publish(const std::string &exchange_name, T &&t) {
    envelope e;
    e.payload = std::forward<T>(t);
    publish_to_exchange(exchange_name, e);
  }

  virtual actor_address add_actor(const std::string &actor_name, const actor_ptr a) = 0;
  virtual void send_envelope(const actor_address &target, const envelope &e) = 0;
  virtual void send_envelope(const actor_address &target, const envelope &&e) = 0;
  virtual void stop_actor(const actor_address &addr) = 0;
  virtual actor_weak get_actor(const actor_address &addr) const = 0;
  virtual actor_weak get_actor(const std::string &name) const = 0;
  virtual actor_address get_address(const std::string &name) const = 0;
  virtual void create_exchange(const std::string &name) = 0;
  virtual void subscribe_to_exchange(const std::string &name) = 0;
  virtual void publish_to_exchange(const std::string &exchange, const envelope &e) = 0;
  virtual void publish_to_exchange(const std::string &exchange, const envelope &&e) = 0;
  virtual std::string name() const = 0;
};
} // namespace yaaf
