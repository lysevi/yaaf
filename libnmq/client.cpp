#include <libnmq/client.h>

using namespace nmq;

Client::Client(boost::asio::io_service *service, const Connection::Params &_params)
    : Connection(service, _params) {}

Client::~Client() {}

bool Client::is_connected() {
  return _loginConfirmed;
}

void Client::connect() {
  this->async_connect();
  while (!this->is_connected()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
  }
}

void Client::connectAsync() {
  this->async_connect();
}

void Client::disconnect() {
  return Connection::disconnect();
}

void Client::onConnect() {
  logger("client(", _params.login, "):: send login.");
  queries::Login lg(this->_params.login);
  _loginConfirmed = false;
  network::Message_ptr mptr = lg.toNetworkMessage();
  this->send(mptr);

  logger("client(", _params.login, "):: send login sended.");
}

void Client::onNewMessage(const network::Message_ptr &d, bool &cancel) {
  auto hdr = d->cast_to_header();

  switch (hdr->kind) {
  case MessageKinds::OK : {
    logger("client (", _params.login, "): recv ok");
    auto cs = queries::Ok(d);

    {
      std::lock_guard<std::shared_mutex> lg(_locker);
      auto it = _queries.find(cs.id);
      if (it == _queries.end()) {
        THROW_EXCEPTION("unknow _query id=", cs.id);
      } else {
        it->second.locker->unlock();
        _queries.erase(it);
      }
    }
    break;
  }

  case MessageKinds::LOGIN_CONFIRM: {
    logger_info("client (", _params.login, "): login confirm");
    auto lc = queries::LoginConfirm(d);
    _id = lc.id;
    _loginConfirmed = true;
    break;
  }
  case MessageKinds::LOGIN_FAILED: {
    logger_info("client (", _params.login, "): login failed");
    this->_loginConfirmed = false;
    this->disconnect();
    break;
  }
  default:
    THROW_EXCEPTION("client (", _params.login, "):unknow message kind: ", hdr->kind);
  }
}

void Client::onNetworkError(const network::Message_ptr &d,
                            const boost::system::error_code &err) {
  bool isError = err == boost::asio::error::operation_aborted ||
                 err == boost::asio::error::connection_reset ||
                 err == boost::asio::error::eof;
  if (isError && !isStoped) {
    int errCode = err.value();
    std::string msg = err.message();
    logger_fatal("client (", _params.login, "): network error (", errCode, ") - ", msg);
  }
  _loginConfirmed = false;
}

void Client::waitAll() const {
  while (true) {
    std::lock_guard<std::shared_mutex> lg(_locker);
    if (_queries.empty()) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
  }
}

// void Client::publish(const PublishParams &settings, const std::vector<uint8_t> &data,
//                     const OperationType ot) {
//  AsyncOperationResult qr;
//  {
//    std::lock_guard<std::shared_mutex> lg(_locker);
//    auto msgId = getNextId();
//    queries::Publish pb(settings, data, msgId);
//    _messagePool->append(pb);
//    qr = makeNewQResult(msgId);
//    publish_inner(pb);
//  }
//  if (ot == OperationType::Sync) {
//    qr.locker->lock();
//  }
//}

void Client::send(const network::Message_ptr &nd) {
  if (_async_connection != nullptr) {
    _async_connection->send(nd);
  }
}


