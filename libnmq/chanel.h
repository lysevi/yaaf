#pragma once
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace nmq {

template <typename Arg, typename Result> struct BaseIOChanel {
  struct Sender {
    Sender(BaseIOChanel<Arg, Result> &bc, nmq::Id id_) : chanel(bc) { id = id_; }
    BaseIOChanel<Arg, Result> &chanel;
    nmq::Id id;
  };

  struct ErrorCode {
    const boost::system::error_code &ec;
  };

  struct IOListener;
  struct IOConnection;

  class IOManager {
  public:
    using io_chanel_type = typename BaseIOChanel<Arg, Result>;

    virtual void start(){};

    virtual void stop() {
      std::lock_guard<std::mutex> lg(_chanels_locker);
      for (auto l : _listeners) {
        l.second->stopListener();
      }

      for (auto c : _connections) {
        c.second->stopConnection();
      }
    };

    Id addListener( IOListener *l) {
      std::lock_guard<std::mutex> lg(_chanels_locker);
      auto id = _id.fetch_add(1);
      _listeners[id] = l;
      return id;
    }

    Id addConnection( IOConnection *c) {
      std::lock_guard<std::mutex> lg(_chanels_locker);
      auto id = _id.fetch_add(1);
      _connections[id] = c;
      return id;
    }

    void rmListener(Id id) {
      std::lock_guard<std::mutex> lg(_chanels_locker);
      _listeners.erase(id);
    }

    void rmConnection(Id id) {
      std::lock_guard<std::mutex> lg(_chanels_locker);
      _connections.erase(id);
    }

  private:
    std::mutex _chanels_locker;
    std::unordered_map<Id, IOListener *> _listeners;
    std::unordered_map<Id, IOConnection *> _connections;

    std::atomic_uint64_t _id;
  };

  class IOListener : public BaseIOChanel {
  public:
    IOListener(std::shared_ptr<IOManager> manager) : _manager(manager) {}
    virtual ~IOListener() { _manager->rmListener(_id); }
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

    virtual void stopListener() {}

  private:
    Id _id;
    std::shared_ptr<IOManager> _manager;
  };

  class IOConnection : public BaseIOChanel {
  public:
    IOConnection() = delete;
    IOConnection(std::shared_ptr<IOManager> manager) : _manager(manager) {}
    virtual ~IOConnection() { _manager->rmConnection(_id); }

    virtual void onConnected() = 0;
    virtual void onError(const ErrorCode &err) = 0;
    virtual void onMessage(const Result d, bool &cancel) = 0;
    virtual void sendAsync(const Arg message) = 0;

    virtual void startConnection() { _id = _manager->addConnection(this); }

    virtual void stopConnection() {}

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