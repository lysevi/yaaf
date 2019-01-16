#pragma once

#include <libnmq/exports.h>
#include <cstdint>
#include <cstring>
#include <memory>
#include <tuple>

namespace nmq {
namespace network {
#pragma pack(push, 1)

struct message {
  using size_t = uint32_t;
  using kind_t = uint16_t;

  struct header {
    kind_t kind;
  };

  // static const size_t MAX_MESSAGE_SIZE = 1024 * 1024 * 4;
  static const size_t SIZE_OF_SIZE = sizeof(size_t);
  static const size_t SIZE_OF_KIND = sizeof(kind_t);
  // static const size_t MAX_BUFFER_SIZE = MAX_MESSAGE_SIZE - sizeof(message_header);

  size_t *size;
  uint8_t *data;

  message(const message &) = delete;

  message(size_t sz) {
    auto realSize = static_cast<size_t>(sz + SIZE_OF_SIZE);
    data = new uint8_t[realSize * 2];
    memset(data, 0, realSize);
    size = (size_t *)data;
    *size = realSize;
  }

  message(size_t sz, const kind_t &kind_) : message(sz + SIZE_OF_KIND) {
    cast_to_header()->kind = kind_;
  }

  ~message() {
    delete[] data;
    data = nullptr;
  }

  uint8_t *value() { return (data + sizeof(size_t) + sizeof(kind_t)); }

  std::tuple<size_t, uint8_t *> as_buffer() {
    uint8_t *v = reinterpret_cast<uint8_t *>(data);
    auto buf_size = *size;
    return std::tie(buf_size, v);
  }

  header *cast_to_header() {
    return reinterpret_cast<header *>(this->data + SIZE_OF_SIZE);
  }
};

#pragma pack(pop)

using message_ptr = std::shared_ptr<message>;
} // namespace network
} // namespace nmq
