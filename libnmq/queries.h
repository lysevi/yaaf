#pragma once

#include <libnmq/kinds.h>
#include <libnmq/network_message.h>
#include <libnmq/serialisation.h>
#include <libnmq/utils/utils.h>
#include <cstdint>
#include <cstring>

namespace nmq {
namespace queries {

struct Ok {
  uint64_t id;

  using Scheme = serialisation::Scheme<uint64_t>;

  Ok(uint64_t id_) { id = id_; }

  Ok(const NetworkMessage_ptr &nd) { Scheme::read(nd->value(), id); }

  NetworkMessage_ptr toNetworkMessage() const {
    auto neededSize = Scheme::capacity(id);

    auto nd = std::make_shared<NetworkMessage>(
        neededSize, (NetworkMessage::message_kind)MessageKinds::OK);

    Scheme::write(nd->value(), id);
    return nd;
  }
};

struct Login {
  std::string login;

  using Scheme = serialisation::Scheme<std::string>;

  Login(const std::string &login_) { login = login_; }

  Login(const NetworkMessage_ptr &nd) { Scheme::read(nd->value(), login); }

  NetworkMessage_ptr toNetworkMessage() const {
    auto neededSize = Scheme::capacity(login);

    auto nd = std::make_shared<NetworkMessage>(
        neededSize, (NetworkMessage::message_kind)MessageKinds::LOGIN);

    Scheme::write(nd->value(), login);
    return nd;
  }
};

struct LoginConfirm {
  uint64_t id;

  using Scheme = serialisation::Scheme<uint64_t>;

  LoginConfirm(uint64_t id_) { id = id_; }

  LoginConfirm(const NetworkMessage_ptr &nd) { Scheme::read(nd->value(), id); }

  NetworkMessage_ptr toNetworkMessage() const {
    auto neededSize = Scheme::capacity(id);

    auto nd = std::make_shared<NetworkMessage>(
        neededSize, (NetworkMessage::message_kind)MessageKinds::LOGIN_CONFIRM);

    Scheme::write(nd->value(), id);
    return nd;
  }
};
} // namespace queries
} // namespace nmq
