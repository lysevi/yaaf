#include <boost/asio.hpp>

#include <libnmq/network/connection.h>

using namespace nmq;
using namespace nmq::network;

IConnectionConsumer ::~IConnectionConsumer() {
  _connection->eraseConsumer();
}

bool IConnectionConsumer::isConnected() const {
  return _connection->isStarted();
}

bool IConnectionConsumer::isStoped() const {
  return _connection->isStopBegin();
}

void IConnectionConsumer::addConnection(std::shared_ptr<Connection> c) {
  _connection = c;
}

Connection::Connection(boost::asio::io_service *service, const Params &params)
    : _service(service), _params(params) {}

Connection::~Connection() {
  disconnect();
}

void Connection::disconnect() {
  if (!isStoped()) {
    stopBegin();
    _async_io->fullStop();
    stopComplete();
  }
}

void Connection::reconnectOnError(const MessagePtr &d,
                                  const boost::system::error_code &err) {

  {
    if (_consumers != nullptr) {
      _consumers->onNetworkError(d, err);
    }
  }

  if (!isStopBegin() && !isStoped() && _params.auto_reconnection) {
    this->startAsyncConnection();
  }
}

void Connection::startAsyncConnection() {
  startBegin();

  using namespace boost::asio::ip;
  tcp::resolver resolver(*_service);
  tcp::resolver::query query(_params.host, std::to_string(_params.port),
                             tcp::resolver::query::canonical_name);
  tcp::resolver::iterator iter = resolver.resolve(query);

  for (; iter != tcp::resolver::iterator(); ++iter) {
    auto ep = iter->endpoint();
    if (ep.protocol() == tcp::v4()) {
      break;
    }
  }

  if (iter == tcp::resolver::iterator()) {
    THROW_EXCEPTION("hostname not found.");
  }

  tcp::endpoint ep = *iter;
  logger_info("client: start async connection to ", _params.host, ":", _params.port,
              " - ", ep.address().to_string());

  auto self = this->shared_from_this();
  self->_async_io = std::make_shared<AsyncIO>(self->_service);
  self->_async_io->socket().async_connect(ep, [self](auto ec) {
    if (ec) {
      if (!self->isStoped()) {
        self->reconnectOnError(nullptr, ec);
      }
    } else {

      if (self->_async_io->socket().is_open()) {
        logger_info("client: connected.");
        AsyncIO::data_handler_t on_d = [self](auto d, auto cancel) {
          self->onDataReceive(std::move(d), cancel);
        };
        AsyncIO::error_handler_t on_n = [self](auto d, auto err) {
          self->reconnectOnError(d, err);
        };

        self->_async_io->start(on_d, on_n);

        if (self->_consumers != nullptr) {
          self->_consumers->onConnect();
        }
        self->startComplete();
      }
    }
  });
}

void Connection::onDataReceive(MessagePtr &&d, bool &cancel) {
  {
    if (_consumers != nullptr) {
      _consumers->onNewMessage(std::move(d), cancel);
    }
  }
}

void Connection::sendAsync(const MessagePtr &d) {
  if (_async_io) {
    _async_io->send(d);
  }
}

void Connection::addConsumer(const IConnectionConsumerPtr &c) {
  _consumers = c;
  c->addConnection(shared_from_this());
}

void Connection::eraseConsumer() {
  _consumers = nullptr;
}