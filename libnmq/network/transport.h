#pragma once

#include <libnmq/network/connection.h>
#include <libnmq/network/listener.h>
#include <libnmq/network/queries.h>
#include <libnmq/serialization/serialization.h>

#include <functional>

namespace nmq {
namespace network {

template <typename Arg, typename Result> struct BaseIOChanel {
  struct Sender {
    Sender(BaseIOChanel<Arg, Result> &bc, nmq::Id id_) : chanel(bc) { id = id_; }
    BaseIOChanel<Arg, Result> &chanel;
    nmq::Id id;
  };

  struct ErrorCode {
    const boost::system::error_code &ec;
  };

  struct IOListener : public BaseIOChanel {

    virtual void onStartComplete() = 0;
    virtual void onError(const Sender &i, const ErrorCode &err) = 0;
    virtual void onMessage(const Sender &i, const Arg &d, bool &cancel) = 0;
    /**
    result - true for accept, false for failed.
    */
    virtual bool onClient(const Sender &i) = 0;
    virtual void onClientDisconnect(const Sender &i) = 0;
    virtual void sendAsync(nmq::Id client, const Result &message) = 0;
  };

  struct IOConnection : public BaseIOChanel {
    virtual void onConnected() = 0;
    virtual void onError(const ErrorCode &err) = 0;
    virtual void onMessage(const Result &d, bool &cancel) = 0;
    virtual void sendAsync(const Arg &message) = 0;
  };

  BaseIOChanel() { _next_message_id = 0; }

  virtual void start() = 0;
  virtual void stop() = 0;

  uint64_t getNextMessageId() { return _next_message_id.fetch_add(1); }

  std::atomic_uint64_t _next_message_id;
};

template <typename Arg, typename Result> struct Transport {
  using io_chanel_type = typename BaseIOChanel<Arg, Result>;
  using Sender = typename io_chanel_type::Sender;
  using ArgScheme = serialization::ObjectScheme<Arg>;
  using ResultScheme = serialization::ObjectScheme<Arg>;

  struct params {
    boost::asio::io_service *service;
    std::string host;
    unsigned short port;
  };

  struct Listener : public io_chanel_type::IOListener, public nmq::network::Listener {

    Listener() = delete;
    Listener(const Listener &) = delete;
    Listener &operator=(const Listener &) = delete;

    Listener(boost::asio::io_service *service, const params &p)
        : nmq::network::Listener(service, nmq::network::Listener::Params{p.port}) {
      _next_message_id = 0;
    }

    void onStartComplete() override {}
    bool onNewConnection(nmq::network::ListenerClientPtr i) override {
      return onClient(Sender{*this, i->get_id()});
    }

    void onDisconnect(const nmq::network::ListenerClientPtr &i) override {
      onClientDisconnect(Sender{*this, i->get_id()});
    }
    void onNetworkError(nmq::network::ListenerClientPtr i, const network::MessagePtr &,
                        const boost::system::error_code &err) override {
      onError(Sender{*this, i->get_id()}, ErrorCode{err});
    }

    void onNewMessage(nmq::network::ListenerClientPtr i, const network::MessagePtr &d,
                      bool &cancel) override {

      queries::Message<Arg> msg(d);
      onMessage(Sender{*this, i->get_id()}, msg.msg, cancel);
    }

    void sendAsync(nmq::Id client, const Result &message) override {
      queries::Message<Result> msg(getNextMessageId(), message);
      auto nd = msg.getMessage();
      sendTo(client, nd);
    }

    void start() override { nmq::network::Listener::start(); }

    void stop() override { nmq::network::Listener::stop(); }
  };

  struct Connection : public io_chanel_type::IOConnection,
                      public nmq::network::Connection {
    Connection() = delete;
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    Connection(boost::asio::io_service *service, const std::string &login,
               const Transport::params &transport_params)
        : nmq::network::Connection(
              service, nmq::network::Connection::Params(login, transport_params.host,
                                                        transport_params.port)) {}

    void onConnect() override { this->onConnected(); };
    void onNewMessage(const nmq::network::MessagePtr &d, bool &cancel) override {
      queries::Message<Result> msg(d);
      onMessage(msg.msg, cancel);
    }

    void onNetworkError(const nmq::network::MessagePtr &,
                        const boost::system::error_code &err) override {
      onError(ErrorCode{err});
    }

    void sendAsync(const Arg &message) override {
      queries::Message<Arg> msg(getNextMessageId(), message);
      auto nd = msg.getMessage();
      nmq::network::Connection::sendAsync(nd);
    }

    void start() override { nmq::network::Connection::startAsyncConnection(); }

    void stop() override { nmq::network::Connection::disconnect(); }
  };
};
} // namespace network
} // namespace nmq