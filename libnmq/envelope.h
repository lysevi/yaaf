#pragma once

#include <libnmq/exports.h>
#include <libnmq/types.h>
#include <boost/any.hpp>

namespace nmq {
struct envelope;

class actor_address {
public:
  actor_address(const actor_address &other) = default;
  actor_address &operator=(const actor_address &other) = default;
  actor_address() : _id(0) {}
  actor_address(id_t id_) : _id(id_), _addr("null") {}
  actor_address(id_t id_, std::string addr_) : _id(id_), _addr(addr_) {}
  ~actor_address() {}

  bool empty() const { return _id == 0; }
  id_t get_id() const { return _id; }
  std::string to_string() const { return _addr; }

private:
  id_t _id;
  std::string _addr;
};

struct envelope {
  boost::any payload;
  actor_address sender;
};
} // namespace nmq
