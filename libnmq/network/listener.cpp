#include <boost/asio.hpp>
#include <libnmq/network/listener.h>
#include <libnmq/network/listener_client.h>
#include <libnmq/queries.h>
#include <libnmq/utils/utils.h>
#include <functional>
#include <string>

using namespace std::placeholders;
using namespace boost::asio;
using namespace boost::asio::ip;

using namespace nmq;
using namespace nmq::network;

Listener::Listener(boost::asio::io_service *service, Listener::Params p)
    : _service(service), _params(p) {
  _next_id.store(0);
}

Listener::~Listener() {
  stop();
}

void Listener::start() {
  tcp::endpoint ep(tcp::v4(), _params.port);
  auto aio = std::make_shared<network::async_io>(_service);
  _acc = std::make_shared<boost::asio::ip::tcp::acceptor>(*_service, ep);
  start_accept(aio);
  onStartComplete();
  _is_started = true;
}

void Listener::start_accept(network::AsyncIOPtr aio) {
  auto self = shared_from_this();
  _acc->async_accept(aio->socket(),
                     [self, aio](auto ec) { self->handle_accept(self, aio, ec); });
}

void Listener::handle_accept(std::shared_ptr<Listener> self, network::AsyncIOPtr aio,
                             const boost::system::error_code &err) {
  if (self->_begin_stoping) {
    return;
  }
  if (err) {
    if (err == boost::asio::error::operation_aborted ||
        err == boost::asio::error::connection_reset || err == boost::asio::error::eof) {
      aio->full_stop();
      return;
    } else {
      THROW_EXCEPTION("nmq::server: error on accept - ", err.message());
    }
  } else {
    ENSURE(!self->_is_stoped);

    logger_info("server: accept connection.");
    std::shared_ptr<ListenerClient> new_client = nullptr;
    {
      std::lock_guard<std::mutex> lg(self->_locker_connections);
      new_client = std::make_shared<ListenerClient>((Id)self->_next_id, aio, self);

      self->_next_id.fetch_add(1);
    }

    if (true == self->onNewConnection(new_client)) {
      logger_info("server: connection was accepted.");
      std::lock_guard<std::mutex> lg(self->_locker_connections);
      new_client->start();
      logger_info("server: client connection started.");
      self->_connections.push_back(new_client);
    } else {
      logger_info("server: connection was not accepted.");
      aio->full_stop();
    }
  }
  boost::asio::ip::tcp::socket new_sock(*self->_service);
  auto newaio = std::make_shared<network::async_io>(self->_service);
  self->start_accept(newaio);
}

void Listener::stop() {
  _begin_stoping = true;
  if (!_is_stoped) {
    logger("Listener::stop()");
    _acc->close();
    _acc = nullptr;
    if (!_connections.empty()) {
      std::vector<std::shared_ptr<ListenerClient>> local_copy(_connections.begin(),
                                                              _connections.end());
      for (auto con : local_copy) {
        con->close();
      }
      _connections.clear();
    }
    _is_stoped = true;
  }
}

void Listener::erase_client_description(const ListenerClient_Ptr client) {
  std::lock_guard<std::mutex> lg(_locker_connections);
  auto it = std::find_if(_connections.begin(), _connections.end(),
                         [client](auto c) { return c->get_id() == client->get_id(); });
  ENSURE(it != _connections.end());
  onDisconnect(client->shared_from_this());
  _connections.erase(it);
}

void Listener::sendTo(ListenerClient_Ptr i, message_ptr &d) {
  i->sendData(d);
}

void Listener::sendTo(Id id, message_ptr &d) {
  std::lock_guard<std::mutex> lg(this->_locker_connections);
  for (auto c : _connections) {
    if (c->get_id() == id) {
      sendTo(c, d);
      return;
    }
  }
  THROW_EXCEPTION("server: unknow client #", id);
}

void Listener::sendOk(ListenerClient_Ptr i, uint64_t messageId) {
  auto nd = queries::Ok(messageId).toNetworkMessage();
  this->sendTo(i, nd);
}