#pragma once

#include <libnmq/types.h>
#include <libnmq/utils/utils.h>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace nmq {

enum class ErrorsKinds { ALL_LISTENERS_STOPED, Ok };

template <typename Arg, typename Result> struct BaseIOChanel {
  using ArgType = Arg;
  using ResultType = Result;
  struct Sender {
    Sender(BaseIOChanel<Arg, Result> &bc, nmq::Id id_) : chanel(bc) { id = id_; }
    BaseIOChanel<Arg, Result> &chanel;
    nmq::Id id;
  };

  struct ErrorCode {
    ErrorCode(boost::system::error_code e) : error(e), inner_error(ErrorsKinds::Ok) {}

    ErrorCode(ErrorsKinds e) : inner_error(e) {}
    boost::system::error_code error;
    ErrorsKinds inner_error;
  };

  struct IOListener;
  struct IOConnection;

  class IOManager {
  public:
    using io_chanel_type = typename BaseIOChanel<Arg, Result>;

    virtual void start(){};

    virtual void stop() {
      std::lock_guard<std::shared_mutex> lg_lst(_lock_listeners);
      for (auto l : _listeners) {
        l.second->stopListener();
      }

      std::lock_guard<std::shared_mutex> lg_con(_lock_connections);
      for (auto c : _connections) {
        c.second->stopConnection();
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

  private:
    mutable std::shared_mutex _lock_listeners;
    std::unordered_map<Id, IOListener *> _listeners;
    mutable std::shared_mutex _lock_connections;
    std::unordered_map<Id, IOConnection *> _connections;

    std::atomic_uint64_t _id;
  };

  class IOListener : public BaseIOChanel {
  public:
    IOListener(IOManager *manager) : _manager(manager) {}
    virtual ~IOListener() { _manager->rmListener(_id); }
    Id getId() const { return _Id; }
    virtual bool isStopingBegin() const { return _isStopingBegin; }
    virtual void onStartComplete() = 0;
    virtual void onError(const Sender &i, const ErrorCode &err) = 0;
    virtual void onMessage(const Sender &i, const Arg d, bool &cancel) = 0;
    /**
    result - true for accept, false for failed.
    */
    virtual bool onClient(const Sender &i) = 0;
    virtual void onClientDisconnect(const Sender &i) = 0;
    virtual void sendAsync(nmq::Id client, const Result message) = 0;

    virtual void startListener() { _id = _manager->addListener(this); }

    virtual void stopListener() { _isStopingBegin = true; }

  private:
    Id _id;
    IOManager *_manager;
    bool _isStopingBegin = false;
  };

  class IOConnection : public BaseIOChanel {
  public:
    IOConnection() = delete;
    IOConnection(IOManager *manager) : _manager(manager) {}
    virtual ~IOConnection() { _manager->rmConnection(_id); }

    Id getId() const { return _id; }
    virtual void onConnected() = 0;
    virtual void onError(const ErrorCode &err) = 0;
    virtual void onMessage(const Result d, bool &cancel) = 0;
    virtual void sendAsync(const Arg message) = 0;

    virtual void startConnection() { _id = _manager->addConnection(this); }

    virtual void stopConnection() {}

  private:
    Id _id;
    IOManager *_manager;
  };

  BaseIOChanel() { _next_message_id = 0; }

  virtual void start() = 0;
  virtual void stop() = 0;

  uint64_t getNextMessageId() { return _next_message_id.fetch_add(1); }

  std::atomic_uint64_t _next_message_id;
};
} // namespace nmq