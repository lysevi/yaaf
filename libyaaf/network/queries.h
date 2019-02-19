#pragma once

#include <libyaaf/network/kinds.h>
#include <libyaaf/network/message.h>
#include <libyaaf/serialization/serialization.h>
#include <libyaaf/types.h>
#include <cstdint>
#include <cstring>

namespace yaaf {
namespace network {
namespace queries {

struct ok {
  uint64_t id;

  using BinaryRW = serialization::binary_io<uint64_t>;

  ok(uint64_t id_) { id = id_; }

  ok(const network::message_ptr &nd) { BinaryRW::read(nd->value(), id); }

  ok(network::message_ptr &&nd) { BinaryRW::read(nd->value(), id); }

  network::message_ptr get_message() const {
    network::message::size_t neededSize =
        static_cast<network::message::size_t>(BinaryRW::capacity(id));

    auto nd = std::make_shared<network::message>(
        neededSize, (network::message::kind_t)messagekinds::OK);

    BinaryRW::write(nd->value(), id);
    return nd;
  }
};

struct login {
  std::string login_str;

  using BinaryRW = serialization::binary_io<std::string>;

  login(const std::string &login_) { login_str = login_; }

  login(const network::message_ptr &nd) { BinaryRW::read(nd->value(), login_str); }

  network::message_ptr get_message() const {
    network::message::size_t neededSize =
        static_cast<network::message::size_t>(BinaryRW::capacity(login_str));

    auto nd = std::make_shared<network::message>(
        neededSize, (network::message::kind_t)messagekinds::LOGIN);

    BinaryRW::write(nd->value(), login_str);
    return nd;
  }
};

struct login_confirm {
  uint64_t id;

  using BinaryRW = serialization::binary_io<uint64_t>;

  login_confirm(uint64_t id_) { id = id_; }

  login_confirm(const network::message_ptr &nd) { BinaryRW::read(nd->value(), id); }

  network::message_ptr get_message() const {
    network::message::size_t neededSize =
        static_cast<network::message::size_t>(BinaryRW::capacity(id));

    auto nd = std::make_shared<network::message>(
        neededSize, (network::message::kind_t)messagekinds::LOGIN_CONFIRM);

    BinaryRW::write(nd->value(), id);
    return nd;
  }
};

struct login_failed {
  uint64_t id;

  using BinaryRW = serialization::binary_io<uint64_t>;

  login_failed(uint64_t id_) { id = id_; }

  login_failed(const network::message_ptr &nd) { BinaryRW::read(nd->value(), id); }

  network::message_ptr get_message() const {
    network::message::size_t neededSize =
        static_cast<network::message::size_t>(BinaryRW::capacity(id));

    auto nd = std::make_shared<network::message>(
        neededSize, (network::message::kind_t)messagekinds::LOGIN_FAILED);

    BinaryRW::write(nd->value(), id);
    return nd;
  }
};

template <typename T> struct packed_message {
  T msg;

  packed_message(const T &msg_)  { msg = msg_; }

  packed_message(T &&msg_) : msg(std::move(msg_)) {}

  packed_message(const network::message_ptr &nd) {
    auto iterator = nd->value();
    msg = serialization::object_packer<T>::unpack(iterator);
  }

  packed_message(network::message_ptr &&nd) {
    auto iterator = nd->value();
    msg = serialization::object_packer<T>::unpack(iterator);
  }

  network::message_ptr get_message() const {
    network::message::size_t neededSize = static_cast<network::message::size_t>(
        serialization::object_packer<T>::capacity(msg));

    auto nd = std::make_shared<network::message>(
        neededSize, (network::message::kind_t)messagekinds::MSG);

    serialization::object_packer<T>::pack(nd->value(), msg);
    return nd;
  }
};

} // namespace queries
} // namespace network
} // namespace yaaf
