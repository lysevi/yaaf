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
    Params() { threads_count = 1; }
    std::shared_ptr<boost::asio::io_service> service;
    unsigned int threads_count;
    std::string host;
    unsigned short port;
  };

  class Manager : public io_chanel_type::IOManager {
  public:
    Manager(const Params &p) : _params(p) {
      ENSURE(_params.threads_count > 0);
      _threads.resize(p.threads_count);
      _params.service = std::make_shared<boost::asio::io_service>();
    }

    boost::asio::io_service *service() { return _params.service.get(); }

    void start() override {
      io_chanel_type::IOManager::start();
      _stop = false;
      for (unsigned int i = 0; i < _params.threads_count; i++) {
        _threads[i] = std::thread([this]() {
          while (!_stop) {
            _params.service->run_one();
          }
        });
      }
    }

    void stop() override {
      io_chanel_type::IOManager::stop();
      _params.service->stop();
      _stop = true;
      for (auto &&t : _threads) {
        t.join();
      }
    }

  private:
    bool _stop = false;
    Params _params;
    std::vector<std::thread> _threads;
  };

  class Listener : public io_chanel_type::IOListener, public network::IListenerConsumer {
  public:
    Listener() = delete;
    Listener(const Listener &) = delete;
    Listener &operator=(const Listener &) = delete;

    Listener(Manager *manager, const Transport::Params &transport_params)
        : io_chanel_type::IOListener(manager) {
      _next_message_id = 0;
      _lstnr = std::make_shared<network::Listener>(
          manager->service(), network::Listener::Params{transport_params.port});
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
      io_chanel_type::IOListener::startListener();

      if (!isListenerExists()) {
        _lstnr->addConsumer(this);
      }
      _lstnr->start();
    }

    void stop() override { _lstnr->stop(); }

  private:
    std::shared_ptr<network::Listener> _lstnr;
  };

  class Connection : public io_chanel_type::IOConnection,
                     public network::IConnectionConsumer {
  public:
    Connection() = delete;
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    Connection(Manager *manager, const std::string &login,
               const Transport::Params &transport_Params)
        : io_chanel_type::IOConnection(manager) {

      _connection = std::make_shared<network::Connection>(
          manager->service(), network::Connection::Params(login, transport_Params.host,
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
    std::shared_ptr<network::Connection> _connection;
  };
};
} // namespace network
} // namespace nmq