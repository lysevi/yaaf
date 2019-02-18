#include <libyaaf/network/listener.h>
#include <libyaaf/network/listener_client.h>
#include <libyaaf/network/queries.h>
#include <libyaaf/utils/utils.h>
#include <boost/asio.hpp>
#include <functional>
#include <string>

using namespace boost::asio;
using namespace boost::asio::ip;

using namespace yaaf;
using namespace yaaf::utils::logging;
using namespace yaaf::network;

abstract_listener_consumer ::~abstract_listener_consumer() {
  _lstnr->erase_consumer();
}

void abstract_listener_consumer::set_listener(const std::shared_ptr<listener> &lstnr) {
  _lstnr = lstnr;
}

void abstract_listener_consumer::send_to(id_t id, network::message_ptr &d) {
  if (!_lstnr->is_stopping_started()) {
    _lstnr->send_to(id, d);
  }
}

void abstract_listener_consumer::stop() {
  _lstnr->stop();
}

listener::listener(boost::asio::io_service *service, listener::params_t p)
    : _service(service), _params(p), _consumer() {
  _next_id.store(0);
}

listener::~listener() {
  stop();
}

void listener::start() {
  initialisation_begin();
  tcp::endpoint ep(tcp::v4(), _params.port);
  auto aio = std::make_shared<network::async_io>(_service);
  _acc = std::make_shared<boost::asio::ip::tcp::acceptor>(*_service, ep);

  if (_consumer != nullptr) {
    _consumer->initialisation_begin();
  }
  start_async_accept(aio);
}

void listener::start_async_accept(network::async_io_ptr aio) {
  auto self = shared_from_this();
  _acc->async_accept(aio->socket(),
                     [self, aio](auto ec) { self->OnAcceptHandler(self, aio, ec); });
  if (self->is_stopping_started()) {
    return;
  }
  initialisation_complete();
  if (_consumer != nullptr) {
    _consumer->initialisation_complete();
  }
}

void listener::OnAcceptHandler(std::shared_ptr<listener> self, network::async_io_ptr aio,
                               const boost::system::error_code &err) {
  if (self->is_stopping_started()) {
    return;
  }
  if (err) {
    if (err == boost::asio::error::operation_aborted ||
        err == boost::asio::error::connection_reset || err == boost::asio::error::eof) {
      aio->fullStop();
      return;
    } else {
      THROW_EXCEPTION("listener: error on accept - ", err.message());
    }
  } else {
    ENSURE(!self->is_stoped());

    logger_info("listener: accept connection");
    std::shared_ptr<listener_client> new_client = nullptr;
    {
      std::lock_guard<std::mutex> lg(self->_locker_connections);
      new_client = std::make_shared<listener_client>(self->_next_id.load(), aio, self);

      self->_next_id.fetch_add(1);
    }
    bool connectionAccepted = false;
    if (self->_consumer != nullptr) {
      connectionAccepted = self->_consumer->on_new_connection(new_client);
    }
    if (true == connectionAccepted) {
      logger_info("listener: connection was accepted.");
      std::lock_guard<std::mutex> lg(self->_locker_connections);
      new_client->start();
      logger_info("listener: client connection started.");
      self->_connections.push_back(new_client);
    } else {
      logger_info("listener: connection was not accepted.");
      aio->fullStop();
    }
  }

  boost::asio::ip::tcp::socket new_sock(*self->_service);
  auto newaio = std::make_shared<network::async_io>(self->_service);
  if (self->is_stopping_started()) {
    return;
  }
  self->start_async_accept(newaio);
}

void listener::stop() {
  if (!is_stoped()) {
    stopping_started();
    logger("listener::stop()");

    if (_consumer != nullptr) {
      _consumer->stopping_started();
    }

    auto local_copy = [this]() {
      std::lock_guard<std::mutex> lg(_locker_connections);
      return std::vector<std::shared_ptr<listener_client>>(_connections.begin(),
                                                           _connections.end());
    }();

    for (auto con : local_copy) {
      con->close();
    }

    if (_consumer != nullptr) {
      _consumer->stopping_completed();
    }

    _acc->close();
    _acc = nullptr;
    stopping_completed();
  }
}

void listener::erase_client_description(const listener_client_ptr client) {
  bool locked_localy = _locker_connections.try_lock();
  auto it = std::find_if(_connections.cbegin(), _connections.cend(),
                         [client](auto c) { return c->get_id() == client->get_id(); });
  if (it == _connections.cend()) {
    THROW_EXCEPTION("delete error");
  }
  if (_consumer != nullptr) {
    _consumer->on_disconnect(client->shared_from_this());
  }
  _connections.erase(it);
  if (locked_localy) {
    _locker_connections.unlock();
  }
  client->stopping_completed();
}

void listener::send_to(listener_client_ptr i, message_ptr &d) {
  i->send_data(d);
}

void listener::send_to(id_t id, message_ptr &d) {
  std::lock_guard<std::mutex> lg(this->_locker_connections);
  for (const auto &c : _connections) {
    if (c->get_id() == id) {
      send_to(c, d);
      return;
    }
  }
  THROW_EXCEPTION("listener: unknow client #", id);
}

void listener::sendOk(listener_client_ptr i, uint64_t messageid) {
  auto nd = queries::ok(messageid).get_message();
  this->send_to(i, nd);
}

void listener::add_consumer(const abstract_listener_consumer_ptr &c) {
  _consumer = c;
  c->set_listener(shared_from_this());
}

void listener::erase_consumer() {
  _consumer = nullptr;
}

void listener::on_network_error(listener_client_ptr i, const network::message_ptr &d,
                                const boost::system::error_code &err) {
  if (_consumer != nullptr) {
    _consumer->on_network_error(i, d, err);
  }
}

void listener::on_new_message(listener_client_ptr i, network::message_ptr &&d,
                              bool &cancel) {
  if (_consumer != nullptr) {
    _consumer->on_new_message(i, std::move(d), cancel);
  }
}
