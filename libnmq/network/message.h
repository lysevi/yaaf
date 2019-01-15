#pragma once

#include <libnmq/exports.h>
#include <cstdint>
#include <cstring>
#include <memory>
#include <tuple>

namespace nmq {
namespace network {
#pragma pack(push, 1)

struct Message {
  using message_size_t = uint32_t;
  using message_kind_t = uint16_t;

  struct Header {
    message_kind_t kind;
  };

  // static const size_t MAX_MESSAGE_SIZE = 1024 * 1024 * 4;
  static const size_t SIZE_OF_SIZE = sizeof(message_size_t);
  static const size_t SIZE_OF_KIND = sizeof(message_kind_t);
  // static const size_t MAX_BUFFER_SIZE = MAX_MESSAGE_SIZE - sizeof(message_header);

  message_size_t *size;
  uint8_t *data;

  Message(const Message &) = delete;

  Message(size_t sz) {
    auto realSize = static_cast<message_size_t>(sz + SIZE_OF_SIZE);
    data = new uint8_t[realSize * 2];
    memset(data, 0, realSize);
    size = (message_size_t *)data;
    *size = realSize;
  }

  Message(size_t sz, const message_kind_t &kind_) : Message(sz + SIZE_OF_KIND) {
    cast_to_header()->kind = kind_;
  }

  ~Message() {
    delete[] data;
    data = nullptr;
  }

  uint8_t *value() { return (data + sizeof(message_size_t) + sizeof(message_kind_t)); }

  std::tuple<message_size_t, uint8_t *> as_buffer() {
    uint8_t *v = reinterpret_cast<uint8_t *>(data);
    auto buf_size = *size;
    return std::tie(buf_size, v);
  }

  Header *cast_to_header() {
    return reinterpret_cast<Header *>(this->data + SIZE_OF_SIZE);
  }
};

#pragma pack(pop)

using Message_ptr = std::shared_ptr<Message>;
} // namespace network
} // namespace nmq
