#pragma once

#include <libyaaf/exports.h>
#include <libyaaf/types.h>
#include <string>
#include <utility>

namespace yaaf {

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
  std::string get_pathname() const { return _pathname; }

  bool operator==(const actor_address &other) const {
    return _id == other._id && _pathname == other._pathname;
  }

private:
  id_t _id;
  std::string _pathname;
};
} // namespace yaaf

namespace std {
template <> class hash<yaaf::actor_address> {
public:
  size_t operator()(const yaaf::actor_address &s) const {
    size_t h_id = std::hash<yaaf::id_t>()(s.get_id());
    size_t h_str = std::hash<std::string>()(s.get_pathname());
    return h_id ^ (h_str << 1);
  }
};

inline std::string to_string(const yaaf::actor_address &a) {
  return a.get_pathname();
}
} // namespace std
