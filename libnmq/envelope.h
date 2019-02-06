#pragma once

#include <libnmq/exports.h>
#include <libnmq/types.h>
#include <boost/any.hpp>

namespace nmq {
struct envelope;
class abstract_context;

class actor_address {
public:
  actor_address(const actor_address &other) = default;
  actor_address &operator=(const actor_address &other) = default;
  actor_address() : _id() {}
  actor_address(id_t id_, std::weak_ptr<abstract_context> ctx_) : _id(id_), _ctx(ctx_) {}
  ~actor_address() {}

  bool empty() const { return _ctx.expired(); }
  id_t get_id() const { return _id; }

  std::shared_ptr<abstract_context> ctx() {
    if (auto p = _ctx.lock()) {
      return p;
    } else {
      return nullptr;
    }
  }

  template <class T> void send(actor_address src, T &&t) const {
    envelope ev;
    ev.payload = t;
    ev.sender = src;
    if (auto p = _ctx.lock()) {
      p->send(*this, ev);
    } else { // TODO send status error?
    }
  }

  EXPORT void stop();

private:
  id_t _id;
  std::weak_ptr<abstract_context> _ctx;
};

struct envelope {
  boost::any payload;

  actor_address sender;
};
} // namespace nmq
