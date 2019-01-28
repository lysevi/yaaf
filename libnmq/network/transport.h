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
    Params() { auto_reconnect = true; }
    std::string host;
    unsigned short port;
    bool auto_reconnect;
  };

  class Manager : public io_chanel_type::IOManager {
  public:
    Manager(const Params &p)
        : _params(p), io_chanel_type::IOManager(io_chanel_type::Params(p.threads_count)) {
    }

    void start() override {
      startBegin();
      io_chanel_type::IOManager::start();
      startComplete();
    }

    void stop() override {
      stopBegin();
      io_chanel_type::IOManager::stop();
      stopComplete();
    }

  private:
    Params _params;
  };

  class Listener : public io_chanel_type::IOListener, public NetListenerConsumer {
  public:
    using io_chanel_type::IOListener::isStartBegin;
    using io_chanel_type::IOListener::isStopBegin;
    using io_chanel_type::IOListener::startBegin;
    using io_chanel_type::IOListener::startComplete;
    using io_chanel_type::IOListener::stopBegin;
    using io_chanel_type::IOListener::stopComplete;
    using io_chanel_type::IOListener::waitStarting;
    using io_chanel_type::IOListener::waitStoping;

    Listener() = delete;
    Listener(const Listener &) = delete;
    Listener &operator=(const Listener &) = delete;

    Listener(std::shared_ptr<Manager> manager, const Transport::Params &transport_params_)
        : io_chanel_type::IOListener(manager), transport_params(transport_params_) {
      _next_message_id = 0;
      _manager = manager;
    }

    ~Listener() {
      if (!isStoped()) {
        stop();
      }
    }

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
      // if (!isStopBegin())
      auto okMsg = queries::Ok(msg.asyncOperationId).getMessage();
      sendTo(i->get_id(), okMsg);

      { onMessage(Sender{*this, i->get_id()}, msg.msg, cancel); }
    }

    bool onClient(const Sender &) override { return true; }

    AsyncOperationResult sendAsync(Id client, const Result message) override {
      auto r = _manager->makeAsyncResult();
      queries::Message<Result> msg(getNextMessageId(), r.id, message);

      auto nd = msg.getMessage();
      sendTo(client, nd);

      r.markAsFinished();
      return r;
    }

    void start() override {
      std::lock_guard<std::mutex> lg(_locker);
      startBegin();
      io_chanel_type::IOListener::startListener();

      _lstnr = std::make_shared<NetListener>(getManager()->service(),
                                             NetListener::Params{transport_params.port});

      if (!isListenerExists()) {
        _lstnr->addConsumer(this);
      }
      _lstnr->start();
      _lstnr->waitStarting();
      startComplete();
    }

    void stop() override {
      std::lock_guard<std::mutex> lg(_locker);
      stopBegin();
      io_chanel_type::IOListener::stopListener();
      _lstnr->stop();
      _lstnr->waitStoping();
      _lstnr = nullptr;

      stopComplete();
    }

    bool isStoped() const { return io_chanel_type::IOListener::isStoped(); }
    bool isStarted() const { return io_chanel_type::IOListener::isStarted(); }

  private:
    std::shared_ptr<NetListener> _lstnr;
    Transport::Params transport_params;
    std::mutex _locker;
    std::shared_ptr<Manager> _manager;
  };

  class Connection : public io_chanel_type::IOConnection, public NetConnectionConsumer {
  public:
    Connection() = delete;
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    Connection(std::shared_ptr<Manager> manager,
               const Transport::Params &transport_Params)
        : _transport_Params(transport_Params), io_chanel_type::IOConnection(manager) {
      _manager = manager;
    }

    void onConnect() override {
      io_chanel_type::IOConnection::onConnected();
      this->onConnected();
    };

    void onNewMessage(const MessagePtr &d, bool &cancel) override {
      if (d->header()->kind == (network::Message::kind_t)MessageKinds::OK) {
        queries::Ok okRes(d);
        _manager->markOperationAsFinished(okRes.id);
      } else {
        queries::Message<Result> msg(d);
        onMessage(msg.msg, cancel);
      }
    }

    void onNetworkError(const MessagePtr &,
                        const boost::system::error_code &err) override {

      onError(ErrorCode{err});
    }

    void onError(const ErrorCode &err) override {
      UNUSED(err);
      if (!isStopBegin()) {
        stop();
      }
    }

    AsyncOperationResult sendAsync(const Arg message) override {
      auto r = _manager->makeAsyncResult();
      queries::Message<Arg> msg(getNextMessageId(), r.id, message);
      auto nd = msg.getMessage();

      _connection->sendAsync(nd);

      return r;
    }

    void start() override {
      std::lock_guard<std::mutex> lg(_locker);
      startBegin();

      NetConnection::Params nparams(_transport_Params.host, _transport_Params.port,
                                    _transport_Params.auto_reconnect);
      _connection = std::make_shared<NetConnection>(getManager()->service(), nparams);

      IOConnection::startConnection();
      if (!isConnectionExists()) {
        _connection->addConsumer(this);
      }

      _connection->startAsyncConnection();
    }

    void stop() override {
      std::lock_guard<std::mutex> lg(_locker);
      stopBegin();
      IOConnection::stopConnection();
      _connection->disconnect();
      _connection->waitStoping();
      _connection = nullptr;
      stopComplete();
    }

    bool isStoped() const { return io_chanel_type::IOConnection::isStoped(); }
    bool isStarted() const { return io_chanel_type::IOConnection::isStarted(); }

  private:
    std::shared_ptr<NetConnection> _connection;
    Transport::Params _transport_Params;
    std::mutex _locker;

    std::shared_ptr<Manager> _manager;
  };
};
} // namespace network
} // namespace nmq