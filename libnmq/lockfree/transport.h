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
  using ErrorCode = typename io_chanel_type::ErrorCode;

  struct Params {
    Params() {
      threads_count = 1;
      arg_queue_size = 1;
      result_queue_size = 1;
    }
    std::shared_ptr<io_service> service;
    unsigned int threads_count;
    size_t arg_queue_size;
    size_t result_queue_size;
  };

  class Manager : public io_chanel_type::IOManager {
  public:
    Manager(const Params &p)
        : _params(p), _args(p.arg_queue_size), _results(p.result_queue_size) {
      ENSURE(_params.threads_count > 0);
      ENSURE(_params.arg_queue_size > 0);
      ENSURE(_params.result_queue_size > 0);
      _threads.resize(p.threads_count);
      _params.service = std::make_shared<io_service>();
    }

    io_service *service() { return _params.service.get(); }

    Id addListener(typename io_chanel_type::IOListener *l) override {
      auto res = io_chanel_type::IOManager::addListener(l);
      return res;
    }

    Id addConnection(typename io_chanel_type::IOConnection *c) override {
      auto res = io_chanel_type::IOManager::addConnection(c);
      return res;
    }

    void rmListener(Id id) override {
      io_chanel_type::IOManager::rmListener(id);

      if (listeners_count() == 0) {
        connectionsVisit([](io_chanel_type::IOConnection *c) {
          c->onError(ErrorCode(ErrorsKinds::ALL_LISTENERS_STOPED));
        });
      }
    }

    void rmConnection(Id id) override { io_chanel_type::IOManager::rmConnection(id); }

    void start() override {
      io_chanel_type::IOManager::start();
      _stop = false;
      _params.service->post([this]() { this->queueWorker(); });
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

    bool tryPushArg(Id id, const Arg a) { return _args.tryPush(std::make_pair(id, a)); }
    bool tryPushResult(const Result a) { return _results.tryPush(a); }

    void queueWorker() {
      while (!_args.empty()) {
        std::tuple<bool, std::pair<Id, Arg>> a = _args.tryPop();
        if (std::get<0>(a)) {
          auto arg = std::get<1>(a);
          listenersVisit([arg](typename io_chanel_type::IOListener *l) {
            Sender s{*l, arg.first};
            bool cancel = false;
            l->onMessage(s, arg.second, cancel);
          });
        }
      }

      while (!_results.empty()) {
        std::tuple<bool, Result> a = _results.tryPop();
        if (std::get<0>(a)) {
          auto arg = std::get<1>(a);
          connectionsVisit([arg](typename io_chanel_type::IOConnection *c) {
            bool cancel = false;
            c->onMessage(arg, cancel);
          });
        }
      }
      _params.service->post([this]() { this->queueWorker(); });
    }

  private:
    bool _stop = false;
    Params _params;
    std::vector<std::thread> _threads;

    ArgQueue _args;
    ResultQueue _results;
  };

  class Listener : public io_chanel_type::IOListener {
  public:
    Listener() = delete;
    Listener(const Listener &) = delete;
    Listener &operator=(const Listener &) = delete;

    Listener(Manager *manager, const Transport::Params &transport_params)
        : io_chanel_type::IOListener(manager) {
      _manager = manager;
    }

    void onStartComplete() override {}

    /*void onError(const Sender &i, const ErrorCode &err) override {}

    void onMessage(const Sender &i, const Arg d, bool &cancel) override {}*/

    bool onClient(const Sender &) override { return true; }

    // void onClientDisconnect(const Sender &i) override {}

    void sendAsync(nmq::Id, const Result message) override {
      _manager->tryPushResult(message);
    }

    void startListener() override {
      io_chanel_type::IOListener::startListener();
      onStartComplete();
    }

    void stopListener() override { io_chanel_type::IOListener::stopListener(); }

    void start() { startListener(); }

    void stop() { stopListener(); }

  private:
    Manager *_manager;
  };

  class Connection : public io_chanel_type::IOConnection {
  public:
    Connection() = delete;
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    Connection(Manager *manager, const Transport::Params &)
        : io_chanel_type::IOConnection(manager) {
      _manager = manager;
    }

    void onConnected() override {}
    /*void onError(const ErrorCode &err) override {}
    void onMessage(const Result d, bool &cancel) override {}*/
    void sendAsync(const Arg message) override { _manager->tryPushArg(getId(), message); }

    void startConnection() { io_chanel_type::IOConnection::startConnection(); }

    void stopConnection() { io_chanel_type::IOConnection::stopConnection(); }

    void start() {
      IOConnection::startConnection();
      onConnected();
    }

    void stop() {}

  private:
    Manager *_manager;
  };
};
} // namespace lockfree
} // namespace nmq