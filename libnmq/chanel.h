#pragma once

#include <libnmq/errors.h>
#include <libnmq/types.h>
#include <libnmq/utils/utils.h>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include <boost/asio.hpp>

namespace nmq {

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

    Params(unsigned int threads) {
      ENSURE(threads > 0);
      threads_count = threads;
    }
    unsigned int threads_count;
  };

  class IOManager {
  public:
    using io_chanel_type = typename BaseIOChanel<Arg, Result>;

    IOManager(const Params &p) {
      ENSURE(p.threads_count > 0);
      _threads.resize(p.threads_count);
    }

    boost::asio::io_service *service() { return &_io_service; }

    virtual void start() {
      _stop_io_service = false;
      for (unsigned int i = 0; i < _threads.size(); i++) {
        _threads[i] = std::thread([this]() {
          while (!_stop_io_service) {
            _io_service.run_one();
          }
        });
      }
    };

    virtual void stop() {
      _io_service.stop();
      _stop_io_service = true;
      for (auto &&t : _threads) {
        t.join();
      }

      std::vector<IOListener *> lst;
      std::vector<IOConnection *> cons;
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
        c->onError(ec);
        c->stopConnection();
      }
    };

    virtual Id addListener(IOListener *l) {
      std::lock_guard<std::shared_mutex> lg(_lock_listeners);
      auto id = _id.fetch_add(1);
      _listeners[id] = l;
      return id;
    }

    virtual Id addConnection(IOConnection *c) {
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

    void listenersVisit(std::function<void(IOListener *)> visitor) {
      std::shared_lock<std::shared_mutex> sl(_lock_listeners);
      for (auto v : _listeners) {
        visitor(v.second);
      }
    }

    void connectionsVisit(std::function<void(IOConnection *)> visitor) {
      std::shared_lock<std::shared_mutex> sl(_lock_connections);
      for (auto v : _connections) {
        visitor(v.second);
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
    mutable std::shared_mutex _lock_listeners;
    std::unordered_map<Id, IOListener *> _listeners;
    mutable std::shared_mutex _lock_connections;
    std::unordered_map<Id, IOConnection *> _connections;

    std::atomic_uint64_t _id;

    boost::asio::io_service _io_service;
    std::vector<std::thread> _threads;
    bool _stop_io_service = false;
  };

  class IOListener : public BaseIOChanel {
  public:
    IOListener(IOManager *manager) : _manager(manager) {}
    virtual ~IOListener() { stopListener(); }
    Id getId() const { return _id; }
    virtual bool isStoped() const { return _stoped; }
    virtual void onStartComplete() = 0;
    virtual void onError(const Sender &i, const ErrorCode &err) = 0;
    virtual void onMessage(const Sender &i, const Arg d, bool &cancel) = 0;
    /**
    result - true for accept, false for failed.
    */
    virtual bool onClient(const Sender &i) = 0;
    virtual void onClientDisconnect(const Sender &i) { UNUSED(i); };
    virtual void sendAsync(nmq::Id client, const Result message) = 0;

    virtual void startListener() { _id = _manager->addListener(this); }

    virtual void stopListener() {
      if (!_stoped) {
        _stoped = true;
        _manager->rmListener(_id);
      }
    }

  private:
    Id _id;
    IOManager *_manager;
    bool _stoped = false;
  };

  class IOConnection : public BaseIOChanel {
  public:
    IOConnection() = delete;
    IOConnection(IOManager *manager) : _manager(manager) {}
    virtual ~IOConnection() { stopConnection(); }

    Id getId() const { return _id; }
    bool isStoped() const { return _stoped; }

    virtual void onConnected() = 0;
    virtual void onError(const ErrorCode &err) = 0;
    virtual void onMessage(const Result d, bool &cancel) = 0;
    virtual void sendAsync(const Arg message) = 0;

    virtual void startConnection() { _id = _manager->addConnection(this); }

    virtual void stopConnection() {
      if (!_stoped) {
        _manager->rmConnection(_id);
        _stoped = true;
      }
    }

  private:
    Id _id;
    IOManager *_manager;
    bool _stoped = false;
  };

  BaseIOChanel() { _next_message_id = 0; }

  virtual void start() = 0;
  virtual void stop() = 0;

  uint64_t getNextMessageId() { return _next_message_id.fetch_add(1); }

  std::atomic_uint64_t _next_message_id;
};
} // namespace nmq