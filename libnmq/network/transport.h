#pragma once

#include <libnmq/chanel.h>
#include <libnmq/network/connection.h>
#include <libnmq/network/listener.h>
#include <libnmq/network/queries.h>
#include <libnmq/serialization/serialization.h>

namespace nmq {
namespace network {

template <typename Arg, typename Result> struct Transport {
  using ArgType = Arg;
  using ResultType = Result;

  using io_chanel_type = typename BaseIOChanel<Arg, Result>;
  using Sender = typename io_chanel_type::Sender;
  using ArgScheme = serialization::ObjectScheme<Arg>;
  using ResultScheme = serialization::ObjectScheme<Arg>;

  using NetListener = network::Listener;
  using NetListenerConsumer = network::IListenerConsumer;

  using NetConnection = network::Connection;
  using NetConnectionConsumer = network::IConnectionConsumer;

  struct Params : public io_chanel_type::Params {
    Params() {}
    std::string host;
    unsigned short port;
  };

  class Manager : public io_chanel_type::IOManager {
  public:
    Manager(const Params &p)
        : _params(p), io_chanel_type::IOManager(io_chanel_type::Params(p.threads_count)) {
    }

    void start() override { io_chanel_type::IOManager::start(); }

    void stop() override { io_chanel_type::IOManager::stop(); }

  private:
    Params _params;
  };

  class Listener : public io_chanel_type::IOListener, public NetListenerConsumer {
  public:
    Listener() = delete;
    Listener(const Listener &) = delete;
    Listener &operator=(const Listener &) = delete;

    Listener(Manager *manager, const Transport::Params &transport_params)
        : io_chanel_type::IOListener(manager) {
      _next_message_id = 0;
      _lstnr = std::make_shared<NetListener>(manager->service(),
                                             NetListener::Params{transport_params.port});
    }

    void onStartComplete() override {}

    bool onNewConnection(ListenerClientPtr i) override {
      return onClient(Sender{*this, i->get_id()});
    }

    void onDisconnect(const ListenerClientPtr &i) override {
      onClientDisconnect(Sender{*this, i->get_id()});
    }
    void onNetworkError(ListenerClientPtr i, const MessagePtr &,
                        const boost::system::error_code &err) override {
      onError(Sender{*this, i->get_id()}, ErrorCode{err});
    }

    void onNewMessage(ListenerClientPtr i, const MessagePtr &d, bool &cancel) override {
      queries::Message<Arg> msg(d);
      onMessage(Sender{*this, i->get_id()}, msg.msg, cancel);
    }

    void sendAsync(Id client, const Result message) override {
      queries::Message<Result> msg(getNextMessageId(), message);
      auto nd = msg.getMessage();
      sendTo(client, nd);
    }

    void start() override {
      io_chanel_type::IOListener::startListener();

      if (!isListenerExists()) {
        _lstnr->addConsumer(this);
      }
      _lstnr->start();
    }

    void stop() override {
      io_chanel_type::IOListener::stopListener();
      _lstnr->stop();
    }

    bool isStopingBegin() const override { return _lstnr->isStopingBegin(); }

  private:
    std::shared_ptr<NetListener> _lstnr;
  };

  class Connection : public io_chanel_type::IOConnection, public NetConnectionConsumer {
  public:
    Connection() = delete;
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    Connection(Manager *manager, const Transport::Params &transport_Params)
        : io_chanel_type::IOConnection(manager) {

      NetConnection::Params nparams(transport_Params.host, transport_Params.port);
      _connection = std::make_shared<NetConnection>(manager->service(), nparams);
    }

    void onConnect() override { this->onConnected(); };
    void onNewMessage(const MessagePtr &d, bool &cancel) override {
      queries::Message<Result> msg(d);
      onMessage(msg.msg, cancel);
    }

    void onNetworkError(const MessagePtr &,
                        const boost::system::error_code &err) override {
      onError(ErrorCode{err});
    }

    void sendAsync(const Arg message) override {
      queries::Message<Arg> msg(getNextMessageId(), message);
      auto nd = msg.getMessage();
      _connection->sendAsync(nd);
    }

    void start() override {
      IOConnection::startConnection();
      if (!isConnectionExists()) {
        _connection->addConsumer(this);
      }
      _connection->startAsyncConnection();
    }

    void stop() override {
      IOConnection::stopConnection();
      _connection->disconnect();
    }

  private:
    std::shared_ptr<NetConnection> _connection;
  };
};
} // namespace network
} // namespace nmq