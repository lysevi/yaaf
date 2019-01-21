#pragma once

#include <libnmq/exports.h>
#include <libnmq/utils/utils.h>
#include <array>
#include <cstdint>

namespace nmq {
namespace network {

struct Buffer {
  size_t size;
  uint8_t *data;
};

#pragma pack(push, 1)

class Message {
public:
  using size_t = uint32_t;
  using kind_t = uint16_t;

  struct Header {
    kind_t kind;
  };

  static const size_t MAX_MESSAGE_SIZE = 1024 * 4;
  static const size_t SIZE_OF_SIZE = sizeof(size_t);
  static const size_t SIZE_OF_HEADER = sizeof(Header);
  static const size_t MAX_BUFFER_SIZE = MAX_MESSAGE_SIZE - SIZE_OF_HEADER;

  Message(const Message &other) : data(other.data) {
    size = (size_t *)data.data();
    *size = *other.size;
  }

  Message(size_t sz) {
    ENSURE((sz + SIZE_OF_SIZE) < MAX_MESSAGE_SIZE);
    auto realSize = static_cast<size_t>(sz + SIZE_OF_SIZE);
    std::fill(std::begin(data), std::end(data), uint8_t(0));
    size = (size_t *)data.data();
    *size = realSize;
  }

  Message(size_t sz, const kind_t &kind_) : Message(sz) { header()->kind = kind_; }

  ~Message() {}

  uint8_t *value() { return (data.data() + SIZE_OF_SIZE + sizeof(Header)); }

  Buffer asBuffer() {
    uint8_t *v = reinterpret_cast<uint8_t *>(data.data());
    auto buf_size = *size;
    return Buffer{buf_size, v};
  }

  Header *header() {
    return reinterpret_cast<Header *>(this->data.data() + SIZE_OF_SIZE);
  }

private:
  size_t *size;
  std::array<uint8_t, MAX_MESSAGE_SIZE> data;
};

#pragma pack(pop)

using MessagePtr = std::shared_ptr<Message>;
} // namespace network
} // namespace nmq
