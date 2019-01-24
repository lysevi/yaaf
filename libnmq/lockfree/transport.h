#pragma once

#include <libnmq/chanel.h>
#include <libnmq/lockfree/queue.h>
#include <boost/asio.hpp>

namespace nmq {
namespace lockfree {

using boost::asio::io_service;

template <typename Arg, typename Result, class ArgQueue = FixedQueue<std::pair<Id, Arg>>,
          class ResultQueue = FixedQueue<Result>>
struct Transport {
  using SelfType = Transport<Arg, Result>;
  using ArgType = Arg;
  using ResultType = Result;
  using io_chanel_type = typename BaseIOChanel<Arg, Result>;
  using Sender = typename io_chanel_type::Sender;

  struct Params : public io_chanel_type::Params {
    Params() {
      arg_queue_size = 1;
      result_queue_size = 1;
    }
    size_t arg_queue_size;
    size_t result_queue_size;
  };

  class Manager : public io_chanel_type::IOManager {
  public:
    using io_chanel_type::IOManager::shared_from_this;

    Manager(const Params &p)
        : io_chanel_type::IOManager(io_chanel_type::Params(p.threads_count)), _params(p),
          _args(p.arg_queue_size), _results(p.result_queue_size) {
      ENSURE(_params.threads_count > 0);
      ENSURE(_params.arg_queue_size > 0);
      ENSURE(_params.result_queue_size > 0);
    }

    Id addListener(std::shared_ptr<typename io_chanel_type::IOListener> l) override {
      auto res = io_chanel_type::IOManager::addListener(l);
      return res;
    }

    Id addConnection(std::shared_ptr<typename io_chanel_type::IOConnection> c) override {
      auto res = io_chanel_type::IOManager::addConnection(c);
      return res;
    }

    void rmListener(Id id) override {
      io_chanel_type::IOManager::rmListener(id);

      if (listeners_count() == 0) {
        connectionsVisit([](std::shared_ptr<io_chanel_type::IOConnection> c) {
          c->stopBegin();
          c->onError(ErrorCode(ErrorsKinds::ALL_LISTENERS_STOPED));
          c->stopComplete();
        });
      }
    }

    void rmConnection(Id id) override { io_chanel_type::IOManager::rmConnection(id); }

    void start() override {
      startBegin();
      io_chanel_type::IOManager::start();
      auto self = shared_from_this();
      post([self]() { dynamic_cast<Manager *>(self.get())->queueWorker(); });
      startComplete();
    }

    void stop() override {
      stopBegin();
      io_chanel_type::IOManager::stop();
      stopComplete();
    }

    bool tryPushArg(Id id, const Arg a) { return _args.tryPush(std::make_pair(id, a)); }
    bool tryPushResult(const Result a) { return _results.tryPush(a); }

    void queueWorker() {
      auto self = shared_from_this();
      while (!_args.empty()) {
        auto a = _args.tryPop();
        if (a.ok) {
          auto arg = a.result;

          listenersVisit([self, arg](std::shared_ptr<io_chanel_type::IOListener> l) {
            Sender s{*l, arg.first};
            auto run = [self, s, l, arg]() {
              bool cancel = false;
              l->onMessage(s, arg.second, cancel);
            };
            self->post(run);
          });
        }
      }

      while (!_results.empty()) {
        auto a = _results.tryPop();
        if (a.ok) {
          auto arg = a.result;
          connectionsVisit([self, arg](std::shared_ptr<io_chanel_type::IOConnection> c) {
            auto run = [self, arg, c]() {
              bool cancel = false;
              c->onMessage(arg, cancel);
            };
            self->post(run);
          });
        }
      }

      if (!isStopped()) {
        post([self]() { dynamic_cast<Manager *>(self.get())->queueWorker(); });
      }
    }

  private:
    Params _params;

    ArgQueue _args;
    ResultQueue _results;
  };

  class Listener : public io_chanel_type::IOListener {
  public:
    using io_chanel_type::IOListener::isStarted;
    using io_chanel_type::IOListener::isStoped;
    using io_chanel_type::IOListener::startBegin;
    using io_chanel_type::IOListener::startComplete;
    using io_chanel_type::IOListener::stopBegin;
    using io_chanel_type::IOListener::stopComplete;

    Listener() = delete;
    Listener(const Listener &) = delete;
    Listener &operator=(const Listener &) = delete;

    Listener(std::shared_ptr<Manager> manager,
             const Transport::Params & /*transport_params*/)
        : io_chanel_type::IOListener(manager) {
      _manager = manager;
    }

    bool onClient(const Sender &) override { return true; }

    void sendAsync(nmq::Id, const Result message) override {
      _manager->tryPushResult(message);
    }

    void startListener() override {
      startBegin();
      io_chanel_type::IOListener::startListener();
      startComplete();
    }

    void stopListener() override {
      stopBegin();
      io_chanel_type::IOListener::stopListener();
      stopComplete();
    }

    void start() { startListener(); }

    void stop() { stopListener(); }

  private:
    std::shared_ptr<Manager> _manager;
  };

  class Connection : public io_chanel_type::IOConnection {
  public:
    using io_chanel_type::IOListener::isStarted;
    using io_chanel_type::IOListener::isStoped;
    using io_chanel_type::IOListener::startBegin;
    using io_chanel_type::IOListener::startComplete;
    using io_chanel_type::IOListener::stopBegin;
    using io_chanel_type::IOListener::stopComplete;

    Connection() = delete;
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    Connection(std::shared_ptr<Manager> manager, const Transport::Params &)
        : io_chanel_type::IOConnection(manager) {
      _manager = manager;
    }

    void onError(const ErrorCode &err) override {
      io_chanel_type::IOConnection::onError(err);
    }

    void onConnected() override { io_chanel_type::IOConnection::onConnected(); }
    void sendAsync(const Arg message) override { _manager->tryPushArg(getId(), message); }

    void startConnection() {
      startBegin();
      io_chanel_type::IOConnection::startConnection();
    }

    void stopConnection() { io_chanel_type::IOConnection::stopConnection(); }

    void start() {
      IOConnection::startConnection();
      onConnected();
    }

    void stop() { stopConnection(); }

  private:
    std::shared_ptr<Manager> _manager;
  };
};
} // namespace lockfree
} // namespace nmq