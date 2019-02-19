#include <boost/asio.hpp>

#include <libyaaf/network/dialler.h>

using namespace yaaf;
using namespace yaaf::network;

abstract_dialler ::~abstract_dialler() {
  _connection->erase_consumer();
}

bool abstract_dialler::is_connected() const {
  return _connection->is_started();
}

bool abstract_dialler::is_stoped() const {
  return _connection->is_stopping_started();
}

void abstract_dialler::add_connection(std::shared_ptr<dialler> c) {
  _connection = c;
}

dialler::dialler(boost::asio::io_service *service, const params_t &params)
    : _service(service), _params(params), _consumers() {}

dialler::~dialler() {
  disconnect();
}

void dialler::disconnect() {
  if (!is_stoped()) {
    stopping_started();
    _async_io->fullStop();
    stopping_completed();
  }
}

void dialler::reconnecton_error(const message_ptr &d,
                                const boost::system::error_code &err) {

  {
    if (_consumers != nullptr) {
      _consumers->on_network_error(d, err);
    }
  }

  if (!is_stopping_started() && !is_stoped() && _params.auto_reconnection) {
    this->start_async_connection();
  }
}

void dialler::start_async_connection() {
  if (!is_initialisation_begin()) {
    initialisation_begin();
  }

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
    throw std::logic_error("hostname not found.");
  }

  tcp::endpoint ep = *iter;
  
  auto self = this->shared_from_this();
  self->_async_io = std::make_shared<async_io>(self->_service);
  self->_async_io->socket().async_connect(ep, [self](auto ec) {
    if (ec) {
      if (!self->is_stoped()) {
        self->reconnecton_error(nullptr, ec);
      }
    } else {

      if (self->_async_io->socket().is_open()) {
        async_io::data_handler_t on_d = [self](auto d, auto cancel) {
          self->on_data_receive(std::move(d), cancel);
        };
        async_io::error_handler_t on_n = [self](auto d, auto err) {
          self->reconnecton_error(d, err);
        };

        self->_async_io->start(on_d, on_n);

        if (self->_consumers != nullptr) {
          self->_consumers->on_connect();
        }
        self->initialisation_complete();
      }
    }
  });
}

void dialler::on_data_receive(message_ptr &&d, bool &cancel) {
  {
    if (_consumers != nullptr) {
      _consumers->on_new_message(std::move(d), cancel);
    }
  }
}

void dialler::send_async(const message_ptr &d) {
  if (_async_io) {
    _async_io->send(d);
  }
}

void dialler::add_consumer(const abstract_connection_consumer_ptr &c) {
  _consumers = c;
  c->add_connection(shared_from_this());
}

void dialler::erase_consumer() {
  _consumers = nullptr;
}