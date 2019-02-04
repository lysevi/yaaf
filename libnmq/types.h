#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <string>

namespace nmq {

struct id_t {
  uint64_t value;

  id_t() { value = std::numeric_limits<uint64_t>::max(); }
  id_t(std::uint64_t value_) { value = value_; }
  id_t(const id_t &other) : value(other.value) {}

  id_t &operator=(const id_t &other) {
    if (this != &other) {
      value = other.value;
    }
    return *this;
  }

  bool operator==(const id_t other) const { return value == other.value; }
  bool operator!=(const id_t other) const { return value == other.value; }
  bool operator<(const id_t other) const { return value < other.value; }
  bool operator>(const id_t other) const { return value > other.value; }
  bool operator<=(const id_t other) const { return value <= other.value; }
  bool operator>=(const id_t other) const { return value >= other.value; }

  id_t &operator++() {
    ++value;
    return *this;
  }

  id_t operator++(int) {
    id_t tmp(*this);
    ++(this->value);
    return tmp;
  }
};

inline std::string to_string(const id_t id_) {
  return std::to_string(id_.value);
}

} // namespace nmq

namespace std {
template <> class hash<nmq::id_t> {
public:
  size_t operator()(const nmq::id_t &s) const {
    size_t h = std::hash<uint64_t>()(s.value);
    return h;
  }
};
} // namespace std