#include <libnmq/network/listener.h>
#include <libnmq/network/listener_client.h>
#include <libnmq/utils/utils.h>
#include <functional>
#include <string>

using namespace std::placeholders;
using namespace boost::asio;
using namespace boost::asio::ip;

using namespace nmq;
using namespace nmq::network;

ListenerClient::ListenerClient(Id id_, network::AsyncIOPtr async_io,
                                   std::shared_ptr<Listener> s)
    : id(id_), _listener(s) {
  _async_connection = async_io;
}

ListenerClient::~ListenerClient() {}

void ListenerClient::start() {
  auto self = shared_from_this();

  AsyncIO::onDataRecvHandler on_d = [self](const Message_ptr &d, bool &cancel) {
    self->onDataRecv(d, cancel);
  };

  AsyncIO::onNetworkErrorHandler on_n = [self](auto d, auto err) {
    self->onNetworkError(d, err);
    self->close();
  };

  _async_connection->start(on_d, on_n);
}

void ListenerClient::close() {
  if (_async_connection != nullptr) {
    _async_connection->full_stop();
    _async_connection = nullptr;

    this->_listener->erase_client_description(this->shared_from_this());
  }
}

void ListenerClient::onNetworkError(const Message_ptr &d,
                                      const boost::system::error_code &err) {
  this->_listener->onNetworkError(this->shared_from_this(), d, err);
}

void ListenerClient::onDataRecv(const Message_ptr &d, bool &cancel) {
  _listener->onNewMessage(this->shared_from_this(), d, cancel);
}

void ListenerClient::sendData(const Message_ptr &d) {
  _async_connection->send(d);
}
