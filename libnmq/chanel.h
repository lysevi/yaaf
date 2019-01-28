#pragma once

#include <libnmq/errors.h>
#include <libnmq/types.h>
#include <libnmq/utils/async/locker.h>
#include <libnmq/utils/utils.h>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include <boost/asio.hpp>

namespace nmq {

struct AsyncOperationResult {
  Id id;
  std::shared_ptr<utils::async::locker> locker;

  void wait() { locker->lock(); }

  void markAsFinished() { locker->unlock(); }

  static AsyncOperationResult makeNew(Id id_) {
    AsyncOperationResult result;
    result.locker = std::make_shared<utils::async::locker>();
    result.locker->lock();
    result.id = id_;
    return result;
  }
};

class AsyncOperationsProcess {
public:
  AsyncOperationsProcess() = default;

  AsyncOperationResult makeAsyncResult() {
    std::lock_guard<std::shared_mutex> lg(_asyncOperations_locker);
    __asyncOperationsId++;
    auto result = AsyncOperationResult::makeNew(__asyncOperationsId);
    _asyncOperations[__asyncOperationsId] = result;
    return result;
  }

  void markOperationAsFinished(Id id) {
    std::lock_guard<std::shared_mutex> lg(_asyncOperations_locker);
    auto ao = _asyncOperations.find(id);
    if (ao == _asyncOperations.end()) {
      return;
    }
    ao->second.markAsFinished();
    _asyncOperations.erase(id);
  }

  void waitAllAsyncOperations() {
    for (;;) {
      bool empty = false;
      _asyncOperations_locker.lock_shared();
      empty = _asyncOperations.empty();
      _asyncOperations_locker.unlock_shared();
      if (empty) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

private:
  std::shared_mutex _asyncOperations_locker;
  Id __asyncOperationsId = 0;
  std::unordered_map<Id, AsyncOperationResult> _asyncOperations;
};

template <typename Arg, typename Result> struct BaseIOChanel {
  using ArgType = Arg;
  using ResultType = Result;
  struct Sender {
    Sender(BaseIOChanel<Arg, Result> &bc, nmq::Id id_) : chanel(bc) { id = id_; }
    BaseIOChanel<Arg, Result> &chanel;
    nmq::Id id;
  };

  struct IOListener;
  struct IOConnection;

  struct Params {
    Params() { threads_count = 1; }

    Params(size_t threads) {
      ENSURE(threads > 0);
      threads_count = threads;
    }
    size_t threads_count;
  };

  class IOManager : virtual public std::enable_shared_from_this<IOManager>,
                    public utils::Waitable {
  public:
    using io_chanel_type = typename BaseIOChanel<Arg, Result>;

    IOManager(const Params &p) : _params(p) {
      ENSURE(_params.threads_count > 0);
      _id.store(0);
    }

    boost::asio::io_service *service() { return &_io_service; }

    virtual void start() {

      _stop_io_service = false;

      _threads.resize(_params.threads_count);
      for (unsigned int i = 0; i < _threads.size(); i++) {
        _threads[i] = std::thread([this]() {
          while (!_stop_io_service) {
            _io_service.run_one();
          }
        });
      }
    }

    virtual void stop() {

      _io_service.stop();
      _stop_io_service = true;
      for (auto &&t : _threads) {
        t.join();
      }
      _threads.clear();
      std::vector<std::shared_ptr<IOListener>> lst;
      std::vector<std::shared_ptr<IOConnection>> cons;
      {
        std::shared_lock<std::shared_mutex> lg_lst(_lock_listeners);
        for (auto kv : _listeners) {
          lst.push_back(kv.second);
        }
      }

      {
        std::shared_lock<std::shared_mutex> lg_con(_lock_connections);

        for (auto kv : _connections) {
          cons.push_back(kv.second);
        }
      }
      ErrorCode ec(ErrorsKinds::FULL_STOP);

      for (auto l : lst) {
        Sender s(*l, l->getId());
        l->onError(s, ec);
        l->stopListener();
      }

      for (auto c : cons) {
        c->stopBegin();
        c->onError(ec);
        c->stopConnection();
        c->stopComplete();
      }
    };

    virtual Id addListener(std::shared_ptr<IOListener> l) {
      std::lock_guard<std::shared_mutex> lg(_lock_listeners);
      auto id = _id.fetch_add(1);
      _listeners[id] = l;
      return id;
    }

    virtual Id addConnection(std::shared_ptr<IOConnection> c) {
      std::lock_guard<std::shared_mutex> lg(_lock_connections);
      auto id = _id.fetch_add(1);
      _connections[id] = c;
      return id;
    }

    virtual void rmListener(Id id) {
      std::lock_guard<std::shared_mutex> lg(_lock_listeners);
      _listeners.erase(id);
    }

    virtual void rmConnection(Id id) {
      std::lock_guard<std::shared_mutex> lg(_lock_connections);
      _connections.erase(id);
    }

    void listenersVisit(std::function<bool(std::shared_ptr<IOListener>)> visitor) {
      std::shared_lock<std::shared_mutex> sl(_lock_listeners);
      for (auto v : _listeners) {
        auto continueFlag = visitor(v.second);
        if (!continueFlag) {
          break;
        }
      }
    }

    void connectionsVisit(std::function<bool(std::shared_ptr<IOConnection>)> visitor) {
      std::shared_lock<std::shared_mutex> sl(_lock_connections);
      for (auto v : _connections) {
        auto continueFlag = visitor(v.second);
        if (!continueFlag) {
          break;
        }
      }
    }

    std::shared_ptr<IOConnection> getConnection(const Id id) {
      std::shared_lock<std::shared_mutex> sl(_lock_connections);
      auto result = _connections.find(id);
      if (result != _connections.end()) {
        return result->second;
      } else {
        return nullptr;
      }
    }

    size_t listeners_count() const {
      std::shared_lock<std::shared_mutex> sl(_lock_listeners);
      return _listeners.size();
    }

    size_t connections_count() const {
      std::shared_lock<std::shared_mutex> sl(_lock_connections);
      return _connections.size();
    }

    bool post(std::function<void()> f) {
      if (!_stop_io_service) {
        _io_service.post(f);
        return true;
      }
      return false;
    }

    virtual bool isStopped() const { return _stop_io_service; }

  private:
    Params _params;
    mutable std::shared_mutex _lock_listeners;
    std::unordered_map<Id, std::shared_ptr<IOListener>> _listeners;
    mutable std::shared_mutex _lock_connections;
    std::unordered_map<Id, std::shared_ptr<IOConnection>> _connections;

    std::atomic_uint64_t _id;

    boost::asio::io_service _io_service;
    std::vector<std::thread> _threads;
    bool _stop_io_service = false;
  };

  class IOListener : public BaseIOChanel,
                     public std::enable_shared_from_this<IOListener>,
                     public utils ::Waitable {
  public:
    IOListener(std::shared_ptr<IOManager> manager) : _manager(manager) {}
    virtual ~IOListener() { stopListener(); }

    Id getId() const { return _id; }
    std::shared_ptr<IOManager> getManager() const { return _manager; }

    virtual void onError(const Sender &i, const ErrorCode &err) = 0;
    virtual void onMessage(const Sender &i, const Arg d) = 0;
    /**
    result - true for accept, false for failed.
    */
    virtual bool onClient(const Sender &i) = 0;
    virtual void onClientDisconnect(const Sender &i) { UNUSED(i); };
    virtual AsyncOperationResult sendAsync(nmq::Id client, const Result message) = 0;

    virtual void startListener() { _id = _manager->addListener(shared_from_this()); }

    virtual void stopListener() {
      if (!isStoped()) {
        _manager->rmListener(_id);
      }
    }

  private:
    Id _id;
    std::shared_ptr<IOManager> _manager; // TODO use std::weak_ptr?
  };

  class IOConnection : public BaseIOChanel,
                       public std::enable_shared_from_this<IOConnection>,
                       public utils ::Waitable {
  public:
    IOConnection() = delete;
    IOConnection(std::shared_ptr<IOManager> manager) : _manager(manager) {}
    virtual ~IOConnection() { stopConnection(); }

    Id getId() const { return _id; }
    std::shared_ptr<IOManager> getManager() const { return _manager; }

    virtual void onConnected() { startComplete(); }
    virtual void onError(const ErrorCode &err) { UNUSED(err); };
    virtual void onMessage(const Result d) = 0;
    virtual AsyncOperationResult sendAsync(const Arg message) = 0;

    virtual void startConnection() { _id = _manager->addConnection(shared_from_this()); }

    virtual void stopConnection() {
      if (!isStoped()) {
        _manager->rmConnection(_id);
      }
    }

  private:
    Id _id;
    std::shared_ptr<IOManager> _manager;
  };

  BaseIOChanel() { _next_message_id = 0; }

  virtual void start() = 0;
  virtual void stop() = 0;

  uint64_t getNextMessageId() { return _next_message_id.fetch_add(1); }

  std::atomic_uint64_t _next_message_id;
};
} // namespace nmq