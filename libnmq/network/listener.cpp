#include <libnmq/network/listener.h>
#include <libnmq/network/listener_client.h>
#include <libnmq/network/queries.h>
#include <libnmq/utils/utils.h>
#include <boost/asio.hpp>
#include <functional>
#include <string>

using namespace boost::asio;
using namespace boost::asio::ip;

using namespace nmq;
using namespace nmq::network;

IListenerConsumer ::~IListenerConsumer() {
  _lstnr->eraseConsumer();
}

void IListenerConsumer::setListener(const std::shared_ptr<Listener> &lstnr) {
  _lstnr = lstnr;
}

void IListenerConsumer::sendTo(Id id, network::MessagePtr &d) {
  if (!_lstnr->isStopBegin()) {
    _lstnr->sendTo(id, d);
  }
}

Listener::Listener(boost::asio::io_service *service, Listener::Params p)
    : _service(service), _params(p) {
  _next_id.store(0);
}

Listener::~Listener() {
  stop();
}

void Listener::start() {
  startBegin();
  tcp::endpoint ep(tcp::v4(), _params.port);
  auto aio = std::make_shared<network::AsyncIO>(_service);
  _acc = std::make_shared<boost::asio::ip::tcp::acceptor>(*_service, ep);

  if (_consumer != nullptr) {
    _consumer->startBegin();
  }
  startAsyncAccept(aio);
}

void Listener::startAsyncAccept(network::AsyncIOPtr aio) {
  auto self = shared_from_this();
  _acc->async_accept(aio->socket(),
                     [self, aio](auto ec) { self->OnAcceptHandler(self, aio, ec); });
  if (self->isStopBegin()) {
    return;
  }
  startComplete();
  if (_consumer != nullptr) {
    _consumer->startComplete();
  }
}

void Listener::OnAcceptHandler(std::shared_ptr<Listener> self, network::AsyncIOPtr aio,
                               const boost::system::error_code &err) {
  if (self->isStopBegin()) {
    return;
  }
  if (err) {
    if (err == boost::asio::error::operation_aborted ||
        err == boost::asio::error::connection_reset || err == boost::asio::error::eof) {
      aio->fullStop();
      return;
    } else {
      THROW_EXCEPTION("nmq::server: error on accept - ", err.message());
    }
  } else {
    ENSURE(!self->isStoped());

    logger_info("server: accept connection.");
    std::shared_ptr<ListenerClient> new_client = nullptr;
    {
      std::lock_guard<std::mutex> lg(self->_locker_connections);
      new_client = std::make_shared<ListenerClient>((Id)self->_next_id, aio, self);

      self->_next_id.fetch_add(1);
    }
    bool connectionAccepted = false;
    if (self->_consumer != nullptr) {
      connectionAccepted = self->_consumer->onNewConnection(new_client);
    }
    if (true == connectionAccepted) {
      logger_info("server: connection was accepted.");
      std::lock_guard<std::mutex> lg(self->_locker_connections);
      new_client->start();
      logger_info("server: client connection started.");
      self->_connections.push_back(new_client);
    } else {
      logger_info("server: connection was not accepted.");
      aio->fullStop();
    }
  }

  boost::asio::ip::tcp::socket new_sock(*self->_service);
  auto newaio = std::make_shared<network::AsyncIO>(self->_service);
  if (self->isStopBegin()) {
    return;
  }
  self->startAsyncAccept(newaio);
}

void Listener::stop() {
  if (!isStoped()) {
    stopBegin();
    logger("Listener::stop()");

    if (_consumer != nullptr) {
      _consumer->stopBegin();
    }

    auto local_copy = [this]() {
      std::lock_guard<std::mutex> lg(_locker_connections);
      return std::vector<std::shared_ptr<ListenerClient>>(_connections.begin(),
                                                          _connections.end());
    }();

    for (auto con : local_copy) {
      con->close();
    }

    if (_consumer != nullptr) {
      _consumer->stopComplete();
    }

    _acc->close();
    _acc = nullptr;
    stopComplete();
  }
}

void Listener::eraseClientDescription(const ListenerClientPtr client) {
  bool locked_localy = _locker_connections.try_lock();
  auto it = std::find_if(_connections.cbegin(), _connections.cend(),
                         [client](auto c) { return c->get_id() == client->get_id(); });
  if (it == _connections.cend()) {
    THROW_EXCEPTION("delete error");
  }
  if (_consumer != nullptr) {
    _consumer->onDisconnect(client->shared_from_this());
  }
  _connections.erase(it);
  if (locked_localy) {
    _locker_connections.unlock();
  }
  client->stopComplete();
}

void Listener::sendTo(ListenerClientPtr i, MessagePtr &d) {
  i->sendData(d);
}

void Listener::sendTo(Id id, MessagePtr &d) {
  std::lock_guard<std::mutex> lg(this->_locker_connections);
  for (const auto &c : _connections) {
    if (c->get_id() == id) {
      sendTo(c, d);
      return;
    }
  }
  THROW_EXCEPTION("server: unknow client #", id);
}

void Listener::sendOk(ListenerClientPtr i, uint64_t messageId) {
  auto nd = queries::Ok(messageId).getMessage();
  this->sendTo(i, nd);
}

void Listener::addConsumer(const IListenerConsumerPtr &c) {
  _consumer = c;
  c->setListener(shared_from_this());
}

void Listener::eraseConsumer() {
  _consumer = nullptr;
}

void Listener::onNetworkError(ListenerClientPtr i, const network::MessagePtr &d,
                              const boost::system::error_code &err) {
  if (_consumer != nullptr) {
    _consumer->onNetworkError(i, d, err);
  }
}

void Listener::onNewMessage(ListenerClientPtr i, network::MessagePtr&&d,
                            bool &cancel) {
  if (_consumer != nullptr) {
    _consumer->onNewMessage(i, std::move(d), cancel);
  }
}
