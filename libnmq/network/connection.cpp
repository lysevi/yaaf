#include <libnmq/network/connection.h>

using namespace nmq;
using namespace nmq::network;

Connection::Connection(boost::asio::io_service *service, const Params &params)
    : _service(service), _params(params) {}

Connection::~Connection() {
  disconnect();
}

void Connection::disconnect() {
  if (!isStoped) {
    isStoped = true;
    _async_connection->full_stop();
  }
}

void Connection::reconnectOnError(const Message_ptr &d,
                                  const boost::system::error_code &err) {
  isConnected = false;
  onNetworkError(d, err);
  if (!isStoped && _params.auto_reconnection) {
    this->async_connect();
  }
}

void Connection::async_connect() {
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
  logger_info("client(", _params.login, "): start async connection to ", _params.host,
              ":", _params.port, " - ", ep.address().to_string());

  auto self = this->shared_from_this();
  self->_async_connection = std::make_shared<AsyncIO>(self->_service);
  self->_async_connection->socket().async_connect(ep, [self](auto ec) {
    if (ec) {
      self->reconnectOnError(nullptr, ec);
    } else {
      if (self->_async_connection->socket().is_open()) {
        logger_info("client(", self->_params.login, "): connected.");
        AsyncIO::onDataRecvHandler on_d = [self](auto d, auto cancel) {
          self->dataRecv(d, cancel);
        };
        AsyncIO::onNetworkErrorHandler on_n = [self](auto d, auto err) {
          self->reconnectOnError(d, err);
        };

        self->_async_connection->start(on_d, on_n);
        self->isConnected = true;
        self->onConnect();
      }
    }
  });
}

void Connection::dataRecv(const Message_ptr &d, bool &cancel) {
  onNewMessage(d, cancel);
}
