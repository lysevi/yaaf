#pragma once

#include <libnmq/kinds.h>
#include <libnmq/network/message.h>
#include <libnmq/serialization/serialization.h>
#include <libnmq/utils/utils.h>
#include <cstdint>
#include <cstring>

namespace nmq {
namespace queries {

struct Ok {
  uint64_t id;

  using Scheme = serialization::Scheme<uint64_t>;

  Ok(uint64_t id_) { id = id_; }

  Ok(const network::message_ptr &nd) { Scheme::read(nd->value(), id); }

  network::message_ptr toNetworkMessage() const {
    network::message::size_t neededSize =
        static_cast<network::message::size_t>(Scheme::capacity(id));

    auto nd = std::make_shared<network::message>(
        neededSize, (network::message::kind_t)MessageKinds::OK);

    Scheme::write(nd->value(), id);
    return nd;
  }
};

struct Login {
  std::string login;

  using Scheme = serialization::Scheme<std::string>;

  Login(const std::string &login_) { login = login_; }

  Login(const network::message_ptr &nd) { Scheme::read(nd->value(), login); }

  network::message_ptr toNetworkMessage() const {
    network::message::size_t neededSize =
        static_cast<network::message::size_t>(Scheme::capacity(login));

    auto nd = std::make_shared<network::message>(
        neededSize, (network::message::kind_t)MessageKinds::LOGIN);

    Scheme::write(nd->value(), login);
    return nd;
  }
};

struct LoginConfirm {
  uint64_t id;

  using Scheme = serialization::Scheme<uint64_t>;

  LoginConfirm(uint64_t id_) { id = id_; }

  LoginConfirm(const network::message_ptr &nd) { Scheme::read(nd->value(), id); }

  network::message_ptr toNetworkMessage() const {
    network::message::size_t neededSize =
        static_cast<network::message::size_t>(Scheme::capacity(id));

    auto nd = std::make_shared<network::message>(
        neededSize, (network::message::kind_t)MessageKinds::LOGIN_CONFIRM);

    Scheme::write(nd->value(), id);
    return nd;
  }
};

struct LoginFailed {
  uint64_t id;

  using Scheme = serialization::Scheme<uint64_t>;

  LoginFailed(uint64_t id_) { id = id_; }

  LoginFailed(const network::message_ptr &nd) { Scheme::read(nd->value(), id); }

  network::message_ptr toNetworkMessage() const {
    network::message::size_t neededSize =
        static_cast<network::message::size_t>(Scheme::capacity(id));

    auto nd = std::make_shared<network::message>(
        neededSize, (network::message::kind_t)MessageKinds::LOGIN_FAILED);

    Scheme::write(nd->value(), id);
    return nd;
  }
};

template <typename T> struct Message {
  uint64_t id;
  T msg;
  using Scheme = serialization::Scheme<uint64_t>;

  Message(uint64_t id_, const T &msg_) {
    id = id_;
    msg = msg_;
  }

  Message(const network::message_ptr &nd) {
    auto iterator = nd->value();
    Scheme::read(iterator, id);
    msg = serialization::ObjectScheme<T>::unpack(iterator + Scheme::capacity(id));
  }

  network::message_ptr toNetworkMessage() const {
    auto self_size = Scheme::capacity(id);
    network::message::size_t neededSize = static_cast<network::message::size_t>(
        self_size + serialization::ObjectScheme<T>::capacity(msg));

    auto nd = std::make_shared<network::message>(
        neededSize, (network::message::kind_t)MessageKinds::MSG);

    Scheme::write(nd->value(), id);
    serialization::ObjectScheme<T>::pack(nd->value() + self_size, msg);
    return nd;
  }
};
} // namespace queries
} // namespace nmq
