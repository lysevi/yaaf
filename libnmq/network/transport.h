#pragma once

#include <libnmq/network/connection.h>
#include <libnmq/network/listener.h>
#include <libnmq/queries.h>
#include <libnmq/serialization/serialization.h>

#include <functional>

namespace nmq {
namespace network {

template <typename T> struct base_io_chanel {
  struct sender_type {
    sender_type(base_io_chanel<T> &bc, nmq::Id id_) : chanel(bc) { id = id_; }
    base_io_chanel<T> &chanel;
    nmq::Id id;
  };

  struct error_description {
    const boost::system::error_code &ec;
  };

  struct listener_type : public base_io_chanel {

    virtual void onStartComplete() = 0;
    virtual void onError(const sender_type &i, const error_description &err) = 0;
    virtual void onMessage(const sender_type &i, const T &d, bool &cancel) = 0;
    /**
    result - true for accept, false for failed.
    */
    virtual bool onClient(const sender_type &i) = 0;
    virtual void onClientDisconnect(const sender_type &i) = 0;
    virtual void send_async(nmq::Id client, const T &message) = 0;
  };

  struct connection_type : public base_io_chanel {
    virtual void onConnected() = 0;
    virtual void onError(const error_description &err) = 0;
    virtual void onMessage(const T &d, bool &cancel) = 0;
    virtual void send_async(const T &message) = 0;
  };

  base_io_chanel() { _next_message_id = 0; }

  virtual void start() = 0;
  virtual void stop() = 0;

  uint64_t get_next_message_id() { return _next_message_id.fetch_add(1); }

  std::atomic_uint64_t _next_message_id;
};

template <typename T> struct transport {
  using value_type = T;
  using io_chanel_type = typename base_io_chanel<value_type>;
  using sender_type = typename io_chanel_type::sender_type;
  using ObjectScheme = serialization::ObjectScheme<value_type>;

  struct params {
    boost::asio::io_service *service;
    std::string host;
    unsigned short port;
  };

  struct listener_type : public io_chanel_type::listener_type,
                         public nmq::network::Listener {

    listener_type() = delete;
    listener_type(const listener_type &) = delete;
    listener_type &operator=(const listener_type &) = delete;

    listener_type(boost::asio::io_service *service, const params &p)
        : nmq::network::Listener(service, nmq::network::Listener::Params{p.port}) {
      _next_message_id = 0;
    }

    void onStartComplete() override {}
    bool onNewConnection(nmq::network::ListenerClient_Ptr i) override {
      return onClient(sender_type{*this, i->get_id()});
    }

    void onDisconnect(const nmq::network::ListenerClient_Ptr &i) override {
      onClientDisconnect(sender_type{*this, i->get_id()});
    }
    void onNetworkError(nmq::network::ListenerClient_Ptr i, const network::message_ptr &,
                        const boost::system::error_code &err) override {
      onError(sender_type{*this, i->get_id()}, error_description{err});
    }

    void onNewMessage(nmq::network::ListenerClient_Ptr i, const network::message_ptr &d,
                      bool &cancel) override {

      queries::Message<T> msg(d);
      onMessage(sender_type{*this, i->get_id()}, msg.msg, cancel);
    }

    void send_async(nmq::Id client, const T &message) override {
      nmq::queries::Message<T> msg(get_next_message_id(), message);
      auto nd = msg.toNetworkMessage();
      sendTo(client, nd);
    }

    void start() override { nmq::network::Listener::start(); }

    void stop() override { nmq::network::Listener::stop(); }
  };

  struct connection_type : public io_chanel_type::connection_type,
                           public nmq::network::Connection {
    connection_type() = delete;
    connection_type(const connection_type &) = delete;
    connection_type &operator=(const connection_type &) = delete;

    connection_type(boost::asio::io_service *service, const std::string &login,
                    const transport::params &transport_params)
        : nmq::network::Connection(
              service, nmq::network::Connection::Params(login, transport_params.host,
                                                        transport_params.port)) {}

    void onConnect() override { this->onConnected(); };
    void onNewMessage(const nmq::network::message_ptr &d, bool &cancel) override {
      queries::Message<T> msg(d);
      onMessage(msg.msg, cancel);
    }

    void onNetworkError(const nmq::network::message_ptr &,
                        const boost::system::error_code &err) override {
      onError(error_description{err});
    }

    void send_async(const T &message) override {
      nmq::queries::Message<T> msg(get_next_message_id(), message);
      auto nd = msg.toNetworkMessage();
      nmq::network::Connection::send_async(nd);
    }

    void start() override { nmq::network::Connection::async_connect(); }

    void stop() override { nmq::network::Connection::disconnect(); }
  };
};
} // namespace network
} // namespace nmq