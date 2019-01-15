#include <libnmq/network/listener.h>
#include <libnmq/utils/utils.h>
#include <libnmq/queries.h>
#include <functional>
#include <string>

using namespace std::placeholders;
using namespace boost::asio;
using namespace boost::asio::ip;

using namespace nmq;
using namespace nmq::network;

Listener::ClientConnection::ClientConnection(Id id_, socket_ptr sock_,
                                             std::shared_ptr<Listener> s)
    : id(id_), sock(sock_), _listener(s) {}

Listener::ClientConnection::~ClientConnection() {}

void Listener::ClientConnection::start() {
  auto self = shared_from_this();
  AsyncIO::onDataRecvHandler on_d = [self](const Message_ptr &d, bool &cancel) {
    self->onDataRecv(d, cancel);
  };

  AsyncIO::onNetworkErrorHandler on_n = [self](auto d, auto err) {
    self->onNetworkError(d, err);
    self->close();
  };

  _async_connection = std::make_shared<AsyncIO>(on_d, on_n);
  _async_connection->start(self->_listener->_service, sock);
}

void Listener::ClientConnection::close() {
  if (_async_connection != nullptr) {
    _async_connection->full_stop();
    _async_connection = nullptr;

    this->_listener->erase_client_description(this->shared_from_this());
  }
}

void Listener::ClientConnection::onNetworkError(const Message_ptr &d,
                                                const boost::system::error_code &err) {
  this->_listener->onNetworkError(this->shared_from_this(), d, err);
}

void Listener::ClientConnection::onDataRecv(const Message_ptr &d, bool &cancel) {
  _listener->onNewMessage(this->shared_from_this(), d, cancel);
}

void Listener::ClientConnection::sendData(const Message_ptr &d) {
  _async_connection->send(d);
}

/////////////////////////////////////////////////////////////////

Listener::Listener(boost::asio::io_service *service, Listener::Params p)
    : _service(service), _params(p) {
  _next_id.store(0);
}

Listener::~Listener() {
  stop();
}

void Listener::start() {
  tcp::endpoint ep(tcp::v4(), _params.port);
  auto new_socket = std::make_shared<boost::asio::ip::tcp::socket>(*_service);
  _acc = std::make_shared<boost::asio::ip::tcp::acceptor>(*_service, ep);
  start_accept(new_socket);
  onStartComplete();
  _is_started = true;
}

void Listener::start_accept(socket_ptr sock) {
  _acc->async_accept(*sock,
                     std::bind(&handle_accept, this->shared_from_this(), sock, _1));
}

void Listener::erase_client_description(const ClientConnection_Ptr client) {
  std::lock_guard<std::mutex> lg(_locker_connections);
  auto it = std::find_if(_connections.begin(), _connections.end(),
                         [client](auto c) { return c->get_id() == client->get_id(); });
  ENSURE(it != _connections.end());
  onDisconnect(client->shared_from_this());
  _connections.erase(it);
}

void Listener::handle_accept(std::shared_ptr<Listener> self, socket_ptr sock,
                             const boost::system::error_code &err) {
  if (err) {
    if (err == boost::asio::error::operation_aborted ||
        err == boost::asio::error::connection_reset || err == boost::asio::error::eof) {
      return;
    } else {
      THROW_EXCEPTION("nmq::server: error on accept - ", err.message());
    }
  } else {
    ENSURE(!self->_is_stoped);

    logger_info("server: accept connection.");
    std::shared_ptr<ClientConnection> new_client = nullptr;
    {
      std::lock_guard<std::mutex> lg(self->_locker_connections);
      new_client =
          std::make_shared<Listener::ClientConnection>((Id)self->_next_id, sock, self);
      self->_next_id.fetch_add(1);
    }

    if (self->onNewConnection(new_client) == ON_NEW_CONNECTION_RESULT::ACCEPT) {
      logger_info("server: connection was accepted.");
      std::lock_guard<std::mutex> lg(self->_locker_connections);
      new_client->start();
      logger_info("server: client connection started.");
      self->_connections.push_back(new_client);
    } else {
      logger_info("server: connection was not accepted.");
      sock->close();
    }
  }
  socket_ptr new_sock = std::make_shared<boost::asio::ip::tcp::socket>(*self->_service);
  self->start_accept(new_sock);
}

void Listener::stop() {
  if (!_is_stoped) {
    logger("abstract_server::stop()");
    _acc->close();
    _acc = nullptr;
    if (!_connections.empty()) {
      std::vector<std::shared_ptr<ClientConnection>> local_copy(_connections.begin(),
                                                                _connections.end());
      for (auto con : local_copy) {
        con->close();
      }
      _connections.clear();
    }
    _is_stoped = true;
  }
}

void Listener::sendTo(ClientConnection_Ptr i, Message_ptr &d) {
  i->sendData(d);
}

void Listener::sendTo(Id id, Message_ptr &d) {
  std::lock_guard<std::mutex> lg(this->_locker_connections);
  for (auto c : _connections) {
    if (c->get_id() == id) {
      sendTo(c, d);
      return;
    }
  }
  THROW_EXCEPTION("server: unknow client #", id);
}

void Listener::sendOk(ClientConnection_Ptr i, uint64_t messageId) {
  auto nd = queries::Ok(messageId).toNetworkMessage();
  this->sendTo(i, nd);
}