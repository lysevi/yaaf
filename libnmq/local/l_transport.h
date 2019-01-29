#pragma once

#include <libnmq/chanel.h>
#include <libnmq/local/queue.h>
#include <boost/asio.hpp>

namespace nmq {
namespace local {

using boost::asio::io_service;

template <typename Arg, typename Result, class ArgQueue = Queue<std::pair<Id, Arg>>,
          class ResultQueue = Queue<Result>>
struct Transport {
  using SelfType = Transport<Arg, Result>;
  using ArgType = Arg;
  using ResultType = Result;
  using io_chanel_type = BaseIOChanel<Arg, Result>;
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

    std::shared_ptr<Manager> shared_self() {
      auto self = shared_from_this();
      return std::dynamic_pointer_cast<Manager>(self);
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
      auto self = shared_self();
      post([self]() { self.get()->queueWorker(); });
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
        auto self = shared_self();
        post([self, id, a, aor]() { self.get()->pushArgLoop(id, a, aor); });
      }
    }

    void pushResulLoop(const nmq::Id id, const Result a, Id aor);

    void pushArg(Id id, const Arg a, Id aor) {
      auto self = shared_self();
      post([self, id, a, aor]() { self->pushArgLoop(id, a, aor); });
    }

    void pushResult(const nmq::Id id, const Result a, Id aor) {
      auto self = shared_self();
      post([self, id, a, aor]() { self->pushResulLoop(id, a, aor); });
    }

    void queueWorker() {
      auto self = shared_self();

      if (!_args.empty()) {

        listenersVisit([self](std::shared_ptr<io_chanel_type::IOListener> l) {
          auto tptr = std::dynamic_pointer_cast<typename Transport::Listener>(l);

          if (!tptr->isBusy()) {
            return tptr->run(self->_args);
          }
          return true; // break visitors' loop
        });
      }

      if (!isStopBegin()) {
        post([self]() { self->queueWorker(); });
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

    bool run(ArgQueue &args) {
      if (!_is_busy) { // TODO make thread safety
        auto a = args.tryPop();
        if (a.ok) {
          std::pair<Id, Arg> arg = a.result;

          Sender s{*this, arg.first};
          _is_busy = true;
          // TODO run on _manager->post
          onMessage(s, std::move(arg.second));
          _is_busy = false;
          return true;
        }
      }
      return false;
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

    std::shared_ptr<Connection> shared_self() {
      auto self = shared_from_this();
      return std::dynamic_pointer_cast<Connection>(self);
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
      auto self = shared_self();

      _manager->post([self]() { self->queueWorker(); });
    }

    void run(ResultQueue &q) {
      if (_is_busy.test_and_set(std::memory_order_acquire)) {
        auto d = q.tryPop();
        if (d.ok) {
          onMessage(std::move(d.result));
        }
        _is_busy.clear(std::memory_order_release);
      }
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

  protected:
    void queueWorker() {
      auto self = shared_self();
      if (self->isStopBegin()) {
        return;
      }
      if (!self->_results.empty()) {
        auto run = [self]() { self->run(self->_results); };
        self->_manager->post(run);
      }
      _manager->post([self]() { self->queueWorker(); });
    }

  private:
    std::shared_ptr<Manager> _manager;

    ResultQueue _results;
    std::atomic_flag _is_busy{ATOMIC_FLAG_INIT};
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

  auto tptr = std::dynamic_pointer_cast<typename Transport::Connection>(target);
  ENSURE(tptr != nullptr);
  ENSURE(tptr->getId() == id);
  if (tptr->isStopBegin() || this->isStopBegin() || tptr->_results.tryPush(a)) {
    this->markOperationAsFinished(aor);
    return;
  }
  auto self = shared_self();
  this->post([=]() {
    auto clbk = [=]() { self->pushResulLoop(id, a, aor); };
    self->post(clbk);
  });
}

} // namespace local
} // namespace nmq