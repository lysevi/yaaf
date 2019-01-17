#pragma once

#include <libnmq/chanel.h>
#include <libnmq/network/connection.h>
#include <libnmq/network/listener.h>
#include <libnmq/network/queries.h>
#include <libnmq/serialization/serialization.h>

namespace nmq {
namespace network {

template <typename Arg, typename Result> struct Transport {
  using io_chanel_type = typename BaseIOChanel<Arg, Result>;
  using Sender = typename io_chanel_type::Sender;
  using ArgScheme = serialization::ObjectScheme<Arg>;
  using ResultScheme = serialization::ObjectScheme<Arg>;

  struct Params {
    boost::asio::io_service *service;
    std::string host;
    unsigned short port;
  };

  class Listener : public io_chanel_type::IOListener,
                   public network::IListenerConsumer,
                   public std::enable_shared_from_this<Transport::Listener> {
  public:
    Listener() = delete;
    Listener(const Listener &) = delete;
    Listener &operator=(const Listener &) = delete;

    Listener(boost::asio::io_service *service, const Transport::Params &p) {
      _next_message_id = 0;
      _lstnr =
          std::make_shared<network::Listener>(service, network::Listener::Params{p.port});
    }

    void onStartComplete() override {}

    bool onNewConnection(network::ListenerClientPtr i) override {
      return onClient(Sender{*this, i->get_id()});
    }

    void onDisconnect(const network::ListenerClientPtr &i) override {
      onClientDisconnect(Sender{*this, i->get_id()});
    }
    void onNetworkError(network::ListenerClientPtr i, const network::MessagePtr &,
                        const boost::system::error_code &err) override {
      onError(Sender{*this, i->get_id()}, ErrorCode{err});
    }

    void onNewMessage(network::ListenerClientPtr i, const network::MessagePtr &d,
                      bool &cancel) override {

      queries::Message<Arg> msg(d);
      onMessage(Sender{*this, i->get_id()}, msg.msg, cancel);
    }

    void sendAsync(Id client, const Result message) override {
      queries::Message<Result> msg(getNextMessageId(), message);
      auto nd = msg.getMessage();
      sendTo(client, nd);
    }

    void start() override {
      if (!isListenerExists()) {
        _lstnr->addConsumer(shared_from_this());
      }
      _lstnr->start();
    }

    void stop() override { _lstnr->stop(); }

  private:
    std::shared_ptr<network::Listener> _lstnr;
  };

  class Connection : public io_chanel_type::IOConnection,
                     public network::IConnectionConsumer,
                     public std::enable_shared_from_this<Connection> {
  public:
    Connection() = delete;
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    Connection(boost::asio::io_service *service, const std::string &login,
               const Transport::Params &transport_Params) {

      _connection = std::make_shared<network::Connection>(
          service, network::Connection::Params(login, transport_Params.host,
                                               transport_Params.port));
    }

    void onConnect() override { this->onConnected(); };
    void onNewMessage(const network::MessagePtr &d, bool &cancel) override {
      queries::Message<Result> msg(d);
      onMessage(msg.msg, cancel);
    }

    void onNetworkError(const network::MessagePtr &,
                        const boost::system::error_code &err) override {
      onError(ErrorCode{err});
    }

    void sendAsync(const Arg message) override {
      queries::Message<Arg> msg(getNextMessageId(), message);
      auto nd = msg.getMessage();
      _connection->sendAsync(nd);
    }

    void start() override {
      if (!isConnectionExists()) {
        _connection->addConsumer(shared_from_this());
      }
      _connection->startAsyncConnection();
    }

    void stop() override { _connection->disconnect(); }

  private:
    std::shared_ptr<network::Connection> _connection;
  };
};
} // namespace network
} // namespace nmq