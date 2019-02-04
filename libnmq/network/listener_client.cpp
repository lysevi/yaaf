#include <libnmq/network/listener.h>
#include <libnmq/network/listener_client.h>
#include <libnmq/utils/utils.h>
#include <boost/asio.hpp>
#include <functional>
#include <string>

using namespace boost::asio;
using namespace boost::asio::ip;

using namespace nmq;
using namespace nmq::network;

listenerClient::listenerClient(id_t id_, network::async_io_ptr async_io,
                               std::shared_ptr<listener> s)
    : id(id_), _listener(s) {
  _async_connection = async_io;
}

listenerClient::~listenerClient() {}

void listenerClient::start() {
  start_begin();
  auto self = shared_from_this();

  async_io::data_handler_t on_d = [self](message_ptr &&d, bool &cancel) {
    self->on_data_recv(std::move(d), cancel);
  };

  async_io::error_handler_t on_n = [self](auto d, auto err) {
    self->on_network_error(d, err);
    self->close();
  };

  _async_connection->start(on_d, on_n);
  start_complete();
}

void listenerClient::close() {
  if (!is_stop_begin() && !is_stoped()) {
    stop_begin(true);
    if (_async_connection != nullptr) {
      _async_connection->fullStop();
      _async_connection = nullptr;
      this->_listener->erase_client_description(this->shared_from_this());
    }
  }
}

void listenerClient::on_network_error(const message_ptr &d,
                                    const boost::system::error_code &err) {
  this->_listener->on_network_error(this->shared_from_this(), d, err);
}

void listenerClient::on_data_recv(message_ptr &&d, bool &cancel) {
  _listener->on_new_message(this->shared_from_this(), std::move(d), cancel);
}

void listenerClient::send_data(const message_ptr &d) {
  _async_connection->send(d);
}
