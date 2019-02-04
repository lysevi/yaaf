#pragma once

#include <libnmq/network/kinds.h>
#include <libnmq/network/message.h>
#include <libnmq/serialization/serialization.h>
#include <libnmq/types.h>
#include <libnmq/utils/utils.h>
#include <cstdint>
#include <cstring>

namespace nmq {
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
  uint64_t id;
  uint64_t asyncOperationid;
  uint64_t clientid;
  T msg;
  using BinaryRW = serialization::binary_io<uint64_t, uint64_t, uint64_t>;

  packed_message(uint64_t id_, id_t asyncOperationid_, id_t client, const T &msg_) {
    id = id_;
    msg = msg_;
    clientid = client.value;
    asyncOperationid = asyncOperationid_.value;
  }

  packed_message(uint64_t id_, id_t asyncOperationid_, id_t client, T &&msg_)
      : id(id_), msg(std::move(msg_)), clientid(client.value),
        asyncOperationid(asyncOperationid_.value) {}

  packed_message(const network::message_ptr &nd) {
    auto iterator = nd->value();
    BinaryRW::read(iterator, id, asyncOperationid, clientid);
    msg = serialization::object_packer<T>::unpack(
        iterator + BinaryRW::capacity(id, asyncOperationid, clientid));
  }

  packed_message(network::message_ptr &&nd) {
    auto iterator = nd->value();
    BinaryRW::read(iterator, id, asyncOperationid, clientid);
    msg = serialization::object_packer<T>::unpack(
        iterator + BinaryRW::capacity(id, asyncOperationid, clientid));
  }

  network::message_ptr get_message() const {
    auto self_size = BinaryRW::capacity(id, asyncOperationid, clientid);
    network::message::size_t neededSize = static_cast<network::message::size_t>(
        self_size + serialization::object_packer<T>::capacity(msg));

    auto nd = std::make_shared<network::message>(
        neededSize, (network::message::kind_t)messagekinds::MSG);

    BinaryRW::write(nd->value(), id, asyncOperationid, clientid);
    serialization::object_packer<T>::pack(nd->value() + self_size, msg);
    return nd;
  }
};
} // namespace queries
} // namespace network
} // namespace nmq
