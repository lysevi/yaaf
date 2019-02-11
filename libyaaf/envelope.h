#pragma once

#include <libyaaf/exports.h>
#include <libyaaf/types.h>
#include <libyaaf/payload.h>

namespace yaaf {
struct envelope;

class actor_address {
public:
  actor_address() : _id(0) {}
  actor_address(id_t id_) : _id(id_), _addr("null") {}
  actor_address(id_t id_, std::string addr_) : _id(id_), _addr(addr_) {}
  actor_address(const actor_address &other) : _id(other._id), _addr(other._addr) {}

  actor_address &operator=(const actor_address &other) {
    if (this != &other) {
      _id = other._id;
      _addr = other._addr;
    }
    return *this;
  }
  ~actor_address() {}

  bool empty() const { return _id == 0; }
  id_t get_id() const { return _id; }
  std::string to_string() const { return _addr; }

private:
  id_t _id;
  std::string _addr;
};

struct envelope {
  payload_t payload;
  actor_address sender;
};
} // namespace yaaf
