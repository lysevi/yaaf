#pragma once

#include <libnmq/network/kinds.h>
#include <libnmq/network/message.h>
#include <libnmq/serialization/serialization.h>
#include <libnmq/utils/utils.h>
#include <cstdint>
#include <cstring>

namespace nmq {
namespace network {
namespace queries {

struct Ok {
  uint64_t id;

  using Scheme = serialization::Scheme<uint64_t>;

  Ok(uint64_t id_) { id = id_; }

  Ok(const network::MessagePtr &nd) { Scheme::read(nd->value(), id); }

  network::MessagePtr getMessage() const {
    network::Message::size_t neededSize =
        static_cast<network::Message::size_t>(Scheme::capacity(id));

    auto nd = std::make_shared<network::Message>(
        neededSize, (network::Message::kind_t)MessageKinds::OK);

    Scheme::write(nd->value(), id);
    return nd;
  }
};

struct Login {
  std::string login;

  using Scheme = serialization::Scheme<std::string>;

  Login(const std::string &login_) { login = login_; }

  Login(const network::MessagePtr &nd) { Scheme::read(nd->value(), login); }

  network::MessagePtr getMessage() const {
    network::Message::size_t neededSize =
        static_cast<network::Message::size_t>(Scheme::capacity(login));

    auto nd = std::make_shared<network::Message>(
        neededSize, (network::Message::kind_t)MessageKinds::LOGIN);

    Scheme::write(nd->value(), login);
    return nd;
  }
};

struct LoginConfirm {
  uint64_t id;

  using Scheme = serialization::Scheme<uint64_t>;

  LoginConfirm(uint64_t id_) { id = id_; }

  LoginConfirm(const network::MessagePtr &nd) { Scheme::read(nd->value(), id); }

  network::MessagePtr getMessage() const {
    network::Message::size_t neededSize =
        static_cast<network::Message::size_t>(Scheme::capacity(id));

    auto nd = std::make_shared<network::Message>(
        neededSize, (network::Message::kind_t)MessageKinds::LOGIN_CONFIRM);

    Scheme::write(nd->value(), id);
    return nd;
  }
};

struct LoginFailed {
  uint64_t id;

  using Scheme = serialization::Scheme<uint64_t>;

  LoginFailed(uint64_t id_) { id = id_; }

  LoginFailed(const network::MessagePtr &nd) { Scheme::read(nd->value(), id); }

  network::MessagePtr getMessage() const {
    network::Message::size_t neededSize =
        static_cast<network::Message::size_t>(Scheme::capacity(id));

    auto nd = std::make_shared<network::Message>(
        neededSize, (network::Message::kind_t)MessageKinds::LOGIN_FAILED);

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

  Message(const network::MessagePtr &nd) {
    auto iterator = nd->value();
    Scheme::read(iterator, id);
    msg = serialization::ObjectScheme<T>::unpack(iterator + Scheme::capacity(id));
  }

  network::MessagePtr getMessage() const {
    auto self_size = Scheme::capacity(id);
    network::Message::size_t neededSize = static_cast<network::Message::size_t>(
        self_size + serialization::ObjectScheme<T>::capacity(msg));

    auto nd = std::make_shared<network::Message>(
        neededSize, (network::Message::kind_t)MessageKinds::MSG);

    Scheme::write(nd->value(), id);
    serialization::ObjectScheme<T>::pack(nd->value() + self_size, msg);
    return nd;
  }
};
} // namespace queries
} // namespace network
} // namespace nmq
