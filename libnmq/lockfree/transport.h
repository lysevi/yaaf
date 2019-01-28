#pragma once

#include <libnmq/chanel.h>
#include <libnmq/lockfree/queue.h>
#include <boost/asio.hpp>

namespace nmq {
namespace lockfree {

using boost::asio::io_service;

template <typename Arg, typename Result, class ArgQueue = Queue<std::pair<Id, Arg>>,
          class ResultQueue = Queue<Result>>
struct Transport {
  using SelfType = Transport<Arg, Result>;
  using ArgType = Arg;
  using ResultType = Result;
  using io_chanel_type = typename BaseIOChanel<Arg, Result>;
  using Sender = typename io_chanel_type::Sender;

  struct Params : public io_chanel_type::Params {
    Params() {
      arg_queue_size = 10;
      result_queue_size = 10;
    }
    size_t arg_queue_size;
    size_t result_queue_size;
  };
  class Connection;
  class Listener;

  class Manager : public io_chanel_type::IOManager, public AsyncOperationsProcess {
  public:
    using io_chanel_type::IOManager::shared_from_this;

    Manager(const Params &p)
        : io_chanel_type::IOManager(io_chanel_type::Params(p.threads_count)), _params(p),
          _args(p.arg_queue_size) {
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
          // c->stopBegin();
          c->onError(ErrorCode(ErrorsKinds::ALL_LISTENERS_STOPED));
          // c->stopComplete();
          return true;
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
      connectionsVisit([](std::shared_ptr<io_chanel_type::IOConnection> c) {
        c->onError(ErrorCode(ErrorsKinds::FULL_STOP));
        return true;
      });

      waitAllAsyncOperations();

      io_chanel_type::IOManager::stop();
      stopComplete();
    }

    void pushArgLoop(Id id, const Arg a, Id aor) {
      if (isStopBegin() || _args.tryPush(std::make_pair(id, a))) {
        this->markOperationAsFinished(aor);
        return;
      } else {
        auto self = shared_from_this();
        post([self, id, a, aor]() {
          dynamic_cast<Manager *>(self.get())->pushArgLoop(id, a, aor);
        });
      }
    }

    void pushResulLoop(const nmq::Id id, const Result a, Id aor);

    void pushArg(Id id, const Arg a, Id aor) {
      auto self = shared_from_this();
      post([self, id, a, aor]() {
        dynamic_cast<Manager *>(self.get())->pushArgLoop(id, a, aor);
      });
    }

    void pushResult(const nmq::Id id, const Result a, Id aor) {
      auto self = shared_from_this();
      post([self, id, a, aor]() {
        dynamic_cast<Manager *>(self.get())->pushResulLoop(id, a, aor);
      });
    }

    void queueWorker() {
      auto self = shared_from_this();

      if (!_args.empty()) {

        listenersVisit([self](std::shared_ptr<io_chanel_type::IOListener> l) {
          auto selfPtr = dynamic_cast<Manager *>(self.get());
          auto tptr = dynamic_cast<typename Transport::Listener *>(l.get());

          if (!tptr->isBusy()) {
            auto a = selfPtr->_args.tryPop();
            if (a.ok) {
              auto arg = a.result;

              Sender s{*l, arg.first};
              //auto run = [self, s, l, arg]() 
			  {
                auto rawPtr = dynamic_cast<typename Transport::Listener *>(l.get());
                bool cancel = false;
                rawPtr->run(s, arg.second, cancel);
              };
              //self->post(run);
              return true;
            } else {
              return false; // break visitors' loop
            }
          }
        });
      }

      connectionsVisit([self](std::shared_ptr<io_chanel_type::IOConnection> c) {
        auto tptr = dynamic_cast<typename Transport::Connection *>(c.get());
        ENSURE(tptr != nullptr);
        if (!tptr->_results.empty()) {
          auto run = [self, c, tptr]() {
            auto arg = tptr->_results.tryPop();
            if (arg.ok) {
              bool cancel = false;
              c->onMessage(arg.result, cancel);
            }
          };
          self->post(run);
          return true;
        }
      });

      if (!isStopBegin()) {
        post([self]() { dynamic_cast<Manager *>(self.get())->queueWorker(); });
      }
    }

  private:
    Params _params;

    ArgQueue _args;
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

    AsyncOperationResult sendAsync(const nmq::Id id, const Result message) override {
      auto r = _manager->makeAsyncResult();
      _manager->pushResult(id, message, r.id);
      return r;
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

    bool isBusy() { return _is_busy; }

    void run(const Sender &i, const Arg d, bool &cancel) {
      ENSURE(!_is_busy);
      _is_busy = true;
      onMessage(i, d, cancel);
      _is_busy = false;
    }

  private:
    std::shared_ptr<Manager> _manager;

    bool _is_busy = false;
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

    Connection(std::shared_ptr<Manager> manager, const Transport::Params &p)
        : _results(p.result_queue_size), io_chanel_type::IOConnection(manager) {
      _manager = manager;
    }

    void onError(const ErrorCode &err) override {
      io_chanel_type::IOConnection::onError(err);
    }

    void onConnected() override { io_chanel_type::IOConnection::onConnected(); }

    AsyncOperationResult sendAsync(const Arg message) override {
      auto r = _manager->makeAsyncResult();
      _manager->pushArg(getId(), message, r.id);
      return r;
    }

    void startConnection() {
      startBegin();
      io_chanel_type::IOConnection::startConnection();
    }

    void stopConnection() { io_chanel_type::IOConnection::stopConnection(); }

    void start() {
      startConnection();
      onConnected();
    }

    void stop() {
      stopBegin();
      stopConnection();
      stopComplete();
    }

    friend Manager;

  private:
    std::shared_ptr<Manager> _manager;

    ResultQueue _results;
  };
};

template <class Arg, class Result, class ArgQueue, class ResultQueue>
void Transport<Arg, Result, ArgQueue, ResultQueue>::Manager::pushResulLoop(
    const nmq::Id id, const Result a, Id aor) {
  auto target = getConnection(id);
  if (target == nullptr) { // TODO notify about it.
    this->markOperationAsFinished(aor);
    return;
  }
  
  auto tptr = dynamic_cast<typename Transport::Connection *>(target.get());
  ENSURE(tptr != nullptr);
  ENSURE(tptr->getId() == id);
  if (tptr->isStopBegin() || this->isStopBegin() || tptr->_results.tryPush(a)) {
    this->markOperationAsFinished(aor);
    return;
  }
  auto self = shared_from_this();
  this->post([=]() {
    auto clbk = [=]() { dynamic_cast<Manager *>(self.get())->pushResulLoop(id, a, aor); };
    dynamic_cast<Manager *>(self.get())->post(clbk);
  });
}

} // namespace lockfree
} // namespace nmq