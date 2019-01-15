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

  Ok(const network::Message_ptr &nd) { Scheme::read(nd->value(), id); }

  network::Message_ptr toNetworkMessage() const {
    auto neededSize = Scheme::capacity(id);

    auto nd = std::make_shared<network::Message>(
        neededSize, (network::Message::message_kind_t)MessageKinds::OK);

    Scheme::write(nd->value(), id);
    return nd;
  }
};

struct Login {
  std::string login;

  using Scheme = serialization::Scheme<std::string>;

  Login(const std::string &login_) { login = login_; }

  Login(const network::Message_ptr &nd) { Scheme::read(nd->value(), login); }

  network::Message_ptr toNetworkMessage() const {
    auto neededSize = Scheme::capacity(login);

    auto nd = std::make_shared<network::Message>(
        neededSize, (network::Message::message_kind_t)MessageKinds::LOGIN);

    Scheme::write(nd->value(), login);
    return nd;
  }
};

struct LoginConfirm {
  uint64_t id;

  using Scheme = serialization::Scheme<uint64_t>;

  LoginConfirm(uint64_t id_) { id = id_; }

  LoginConfirm(const network::Message_ptr &nd) { Scheme::read(nd->value(), id); }

  network::Message_ptr toNetworkMessage() const {
    auto neededSize = Scheme::capacity(id);

    auto nd = std::make_shared<network::Message>(
        neededSize, (network::Message::message_kind_t)MessageKinds::LOGIN_CONFIRM);

    Scheme::write(nd->value(), id);
    return nd;
  }
};

struct LoginFailed {
  uint64_t id;

  using Scheme = serialization::Scheme<uint64_t>;

  LoginFailed(uint64_t id_) { id = id_; }

  LoginFailed(const network::Message_ptr &nd) { Scheme::read(nd->value(), id); }

  network::Message_ptr toNetworkMessage() const {
    auto neededSize = Scheme::capacity(id);

    auto nd = std::make_shared<network::Message>(
        neededSize, (network::Message::message_kind_t)MessageKinds::LOGIN_FAILED);

    Scheme::write(nd->value(), id);
    return nd;
  }
};
} // namespace queries
} // namespace nmq
