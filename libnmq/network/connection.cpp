#include <boost/asio.hpp>

#include <libnmq/network/connection.h>

using namespace nmq;
using namespace nmq::network;

IConnectionConsumer ::~IConnectionConsumer() {}

bool IConnectionConsumer::isConnected() const {
  return _connection->isConnected();
}
bool IConnectionConsumer::isStoped() const {
  return _connection->isStoped();
}

void IConnectionConsumer::addConnection(std::shared_ptr<Connection> c, nmq::Id id) {
  _connection = c;
  _id = id;
}

Connection::Connection(boost::asio::io_service *service, const Params &params)
    : _service(service), _params(params) {
  _next_consumer_id.store(0);
}

Connection::~Connection() {
  disconnect();
}

void Connection::disconnect() {
  if (!_isStoped) {
    _isStoped = true;
    _async_io->fullStop();
  }
}

void Connection::reconnectOnError(const MessagePtr &d,
                                  const boost::system::error_code &err) {
  _isConnected = false;
  {
    std::lock_guard<std::mutex> lg(_locker_consumers);
    for (auto kv : _consumers)
      kv.second->onNetworkError(d, err);
  }
  if (!_isStoped && _params.auto_reconnection) {
    this->startAsyncConnection();
  }
}

void Connection::startAsyncConnection() {
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
  logger_info("client: start async connection to ", _params.host,
              ":", _params.port, " - ", ep.address().to_string());

  auto self = this->shared_from_this();
  self->_async_io = std::make_shared<AsyncIO>(self->_service);
  self->_async_io->socket().async_connect(ep, [self](auto ec) {
    if (ec) {
      self->reconnectOnError(nullptr, ec);
    } else {
      if (self->_async_io->socket().is_open()) {
        logger_info("client: connected.");
        AsyncIO::data_handler_t on_d = [self](auto d, auto cancel) {
          self->onDataReceive(d, cancel);
        };
        AsyncIO::error_handler_t on_n = [self](auto d, auto err) {
          self->reconnectOnError(d, err);
        };

        self->_async_io->start(on_d, on_n);
        self->_isConnected = true;
        {
          std::lock_guard<std::mutex> lg(self->_locker_consumers);
          for (auto kv : self->_consumers) {
            kv.second->onConnect();
          }
        }
      }
    }
  });
}

void Connection::onDataReceive(const MessagePtr &d, bool &cancel) {
  {
    std::lock_guard<std::mutex> lg(_locker_consumers);
    for (auto kv : _consumers) {
      bool cncl = false;
      kv.second->onNewMessage(d, cncl);
      cancel = cancel && cncl;
    }
  }
}

void Connection::sendAsync(const MessagePtr &d) {
  if (_async_io) {
    _async_io->send(d);
  }
}

void Connection::addConsumer(const IConnectionConsumerPtr &c) {
  std::lock_guard<std::mutex> lg(_locker_consumers);
  auto id = _next_consumer_id.fetch_add(1);
  _consumers[id] = c;
  c->addConnection(shared_from_this(), id);
}

void Connection::eraseConsumer(nmq::Id id) {
  std::lock_guard<std::mutex> lg(_locker_consumers);
  _consumers.erase(id);
}