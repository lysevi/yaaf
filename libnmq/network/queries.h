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

struct Ok {
  uint64_t id;

  using BinaryRW = serialization::BinaryReaderWriter<uint64_t>;

  Ok(uint64_t id_) { id = id_; }

  Ok(const network::MessagePtr &nd) { BinaryRW::read(nd->value(), id); }

  Ok(network::MessagePtr &&nd) { BinaryRW::read(nd->value(), id); }

  network::MessagePtr getMessage() const {
    network::Message::size_t neededSize =
        static_cast<network::Message::size_t>(BinaryRW::capacity(id));

    auto nd = std::make_shared<network::Message>(
        neededSize, (network::Message::kind_t)MessageKinds::OK);

    BinaryRW::write(nd->value(), id);
    return nd;
  }
};

struct Login {
  std::string login;

  using BinaryRW = serialization::BinaryReaderWriter<std::string>;

  Login(const std::string &login_) { login = login_; }

  Login(const network::MessagePtr &nd) { BinaryRW::read(nd->value(), login); }

  network::MessagePtr getMessage() const {
    network::Message::size_t neededSize =
        static_cast<network::Message::size_t>(BinaryRW::capacity(login));

    auto nd = std::make_shared<network::Message>(
        neededSize, (network::Message::kind_t)MessageKinds::LOGIN);

    BinaryRW::write(nd->value(), login);
    return nd;
  }
};

struct LoginConfirm {
  uint64_t id;

  using BinaryRW = serialization::BinaryReaderWriter<uint64_t>;

  LoginConfirm(uint64_t id_) { id = id_; }

  LoginConfirm(const network::MessagePtr &nd) { BinaryRW::read(nd->value(), id); }

  network::MessagePtr getMessage() const {
    network::Message::size_t neededSize =
        static_cast<network::Message::size_t>(BinaryRW::capacity(id));

    auto nd = std::make_shared<network::Message>(
        neededSize, (network::Message::kind_t)MessageKinds::LOGIN_CONFIRM);

    BinaryRW::write(nd->value(), id);
    return nd;
  }
};

struct LoginFailed {
  uint64_t id;

  using BinaryRW = serialization::BinaryReaderWriter<uint64_t>;

  LoginFailed(uint64_t id_) { id = id_; }

  LoginFailed(const network::MessagePtr &nd) { BinaryRW::read(nd->value(), id); }

  network::MessagePtr getMessage() const {
    network::Message::size_t neededSize =
        static_cast<network::Message::size_t>(BinaryRW::capacity(id));

    auto nd = std::make_shared<network::Message>(
        neededSize, (network::Message::kind_t)MessageKinds::LOGIN_FAILED);

    BinaryRW::write(nd->value(), id);
    return nd;
  }
};

template <typename T> struct Message {
  uint64_t id;
  Id asyncOperationId;
  Id clientId;
  T msg;
  using BinaryRW = serialization::BinaryReaderWriter<uint64_t, Id, Id>;

  Message(uint64_t id_, Id asyncOperationId_, Id client, const T &msg_) {
    id = id_;
    msg = msg_;
    clientId = client, asyncOperationId = asyncOperationId_;
  }

  Message(uint64_t id_, Id asyncOperationId_, Id client, T &&msg_)
      : id(id_), msg(std::move(msg_)), clientId(client),
        asyncOperationId(asyncOperationId_) {}

  Message(const network::MessagePtr &nd) {
    auto iterator = nd->value();
    BinaryRW::read(iterator, id, asyncOperationId, clientId);
    msg = serialization::ObjectScheme<T>::unpack(
        iterator + BinaryRW::capacity(id, asyncOperationId, clientId));
  }

  Message(network::MessagePtr &&nd) {
    auto iterator = nd->value();
    BinaryRW::read(iterator, id, asyncOperationId, clientId);
    msg = serialization::ObjectScheme<T>::unpack(
        iterator + BinaryRW::capacity(id, asyncOperationId, clientId));
  }

  network::MessagePtr getMessage() const {
    auto self_size = BinaryRW::capacity(id, asyncOperationId, clientId);
    network::Message::size_t neededSize = static_cast<network::Message::size_t>(
        self_size + serialization::ObjectScheme<T>::capacity(msg));

    auto nd = std::make_shared<network::Message>(
        neededSize, (network::Message::kind_t)MessageKinds::MSG);

    BinaryRW::write(nd->value(), id, asyncOperationId, clientId);
    serialization::ObjectScheme<T>::pack(nd->value() + self_size, msg);
    return nd;
  }
};
} // namespace queries
} // namespace network
} // namespace nmq
