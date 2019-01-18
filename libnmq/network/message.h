#pragma once

#include <libnmq/exports.h>
#include <cstdint>
#include <cstring>
#include <memory>
#include <tuple>

namespace nmq {
namespace network {

struct Buffer {
  size_t size;
  uint8_t *data;
};

#pragma pack(push, 1)

struct Message {
  using size_t = uint32_t;
  using kind_t = uint16_t;

  struct Header {
    kind_t kind;
  };

  // static const size_t MAX_MESSAGE_SIZE = 1024 * 1024 * 4;
  static const size_t SIZE_OF_SIZE = sizeof(size_t);
  static const size_t SIZE_OF_KIND = sizeof(kind_t);
  // static const size_t MAX_BUFFER_SIZE = MAX_MESSAGE_SIZE - sizeof(message_header);

  size_t *size;
  uint8_t *data;

  Message(const Message &) = delete;

  Message(size_t sz) {
    auto realSize = static_cast<size_t>(sz + SIZE_OF_SIZE);
    data = new uint8_t[realSize * 2];
    memset(data, 0, realSize);
    size = (size_t *)data;
    *size = realSize;
  }

  Message(size_t sz, const kind_t &kind_) : Message(sz + SIZE_OF_KIND) {
    header()->kind = kind_;
  }

  ~Message() {
    delete[] data;
    data = nullptr;
  }

  uint8_t *value() { return (data + sizeof(size_t) + sizeof(kind_t)); }

  Buffer asBuffer() {
    uint8_t *v = reinterpret_cast<uint8_t *>(data);
    auto buf_size = *size;
    return Buffer{buf_size, v};
  }

  Header *header() { return reinterpret_cast<Header *>(this->data + SIZE_OF_SIZE); }
};

#pragma pack(pop)

using MessagePtr = std::shared_ptr<Message>;
} // namespace network
} // namespace nmq
