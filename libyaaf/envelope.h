#pragma once

#include <libyaaf/exports.h>
#include <libyaaf/payload.h>
#include <libyaaf/types.h>

namespace yaaf {
struct envelope;

class actor_address {
public:
  actor_address() : _id(0) {}
  actor_address(id_t id_) : _id(id_), _pathname("null") {}
  actor_address(id_t id_, std::string pathname_) : _id(id_), _pathname(pathname_) {}
  actor_address(const actor_address &other)
      : _id(other._id), _pathname(other._pathname) {}

  actor_address &operator=(const actor_address &other) {
    if (this != &other) {
      _id = other._id;
      _pathname = other._pathname;
    }
    return *this;
  }
  ~actor_address() {}

  bool empty() const { return _id == 0; }
  id_t get_id() const { return _id; }
  std::string to_string() const { return _pathname; }

  bool operator==(const actor_address &other) const {
    return _id == other._id && _pathname == other._pathname;
  }

private:
  id_t _id;
  std::string _pathname;
};

struct envelope {
  payload_t payload;
  actor_address sender;
};
} // namespace yaaf
