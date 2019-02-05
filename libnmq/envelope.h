#pragma once

#include <libnmq/exports.h>
#include <libnmq/types.h>
#include <boost/any.hpp>

namespace nmq {
class context;
struct envelope;

class actor_address {
public:
  actor_address(const actor_address &other) = default;
  actor_address &operator=(const actor_address &other) = default;
  actor_address() : _id(), _ctx(nullptr) {}
  actor_address(id_t id_, context *ctx_) : _id(id_), _ctx(ctx_) {}
  ~actor_address() {}

  bool empty() const { return _ctx == nullptr; }
  id_t get_id() const { return _id; }
  context *ctx() { return _ctx; }

  template <class T> void send(actor_address src, T &&t) {
    envelope ev;
    ev.payload = t;
    ev.sender = src;
    _ctx->send(*this, ev);
  }

  EXPORT void stop();

private:
  id_t _id;
  context *_ctx;
};

struct envelope {
  boost::any payload;

  actor_address sender;
};
} // namespace nmq
