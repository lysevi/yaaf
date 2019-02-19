#pragma once

#include <libyaaf/exports.h>
#include <array>
#include <cstdint>
#include <cassert>

namespace yaaf {
namespace network {

struct buffer {
  size_t size;
  uint8_t *data;
};

#pragma pack(push, 1)

class message {
public:
  using size_t = uint32_t;
  using kind_t = uint16_t;

  struct header_t {
    kind_t kind;
  };

  static const size_t MAX_MESSAGE_SIZE = 1024 * 4;
  static const size_t SIZE_OF_SIZE = sizeof(size_t);
  static const size_t SIZE_OF_HEADER = sizeof(header_t);
  static const size_t MAX_BUFFER_SIZE = MAX_MESSAGE_SIZE - SIZE_OF_HEADER;

  message(const message &other) : data(other.data) {
    size = (size_t *)data.data();
    *size = *other.size;
  }

  message(size_t sz) {
    assert((sz + SIZE_OF_SIZE + SIZE_OF_HEADER) < MAX_MESSAGE_SIZE);
    auto realSize = static_cast<size_t>(sz + SIZE_OF_SIZE);
    std::fill(std::begin(data), std::end(data), uint8_t(0));
    size = (size_t *)data.data();
    *size = realSize;
  }

  message(size_t sz, const kind_t &kind_) : message(sz) {
    *size += SIZE_OF_HEADER;
    get_header()->kind = kind_;
  }

  ~message() {}

  uint8_t *value() { return (data.data() + SIZE_OF_SIZE + sizeof(header_t)); }

  buffer as_buffer() {
    uint8_t *v = reinterpret_cast<uint8_t *>(data.data());
    auto buf_size = *size;
    return buffer{buf_size, v};
  }

  header_t *get_header() {
    return reinterpret_cast<header_t *>(this->data.data() + SIZE_OF_SIZE);
  }

private:
  size_t *size;
  std::array<uint8_t, MAX_MESSAGE_SIZE> data;
};

#pragma pack(pop)

using message_ptr = std::shared_ptr<message>;
} // namespace network
} // namespace yaaf
