#pragma once

#include <libnmq/async_result.h>
#include <libnmq/errors.h>
#include <libnmq/types.h>
#include <libnmq/utils/utils.h>
#include <atomic>
#include <shared_mutex>
#include <unordered_map>

#include <boost/asio.hpp>

namespace nmq {

template <typename Arg, typename Result> struct base_io_chanel {
  using arg_t = Arg;
  using result_t = Result;
  struct sender {
    sender(base_io_chanel<Arg, Result> &bc, nmq::id_t id_) : chanel(bc), id(id_) {}
    base_io_chanel<Arg, Result> &chanel;
    nmq::id_t id;
  };

  struct io_listener;
  struct io_connection;

  struct params {
    params() { threads_count = 1; }

    params(size_t threads) {
      ENSURE(threads > 0);
      threads_count = threads;
    }
    size_t threads_count;
  };

  class io_manager : virtual public std::enable_shared_from_this<io_manager>,
                    public utils::waitable {
  public:
    using io_chanel_t = typename base_io_chanel<Arg, Result>;

    io_manager(const params &p) : _params(p) {
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
      std::vector<std::shared_ptr<io_listener>> lst;
      std::vector<std::shared_ptr<io_connection>> cons;
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
      ecode ec(errors_kinds::FULL_STOP);

      for (auto l : lst) {
        sender s(*l, l->get_id());
        l->on_error(s, ec);
        l->stop_listener();
      }

      for (auto c : cons) {
        c->stop_begin();
        c->on_error(ec);
        c->stop_connection();
        c->stop_complete();
      }
    };

    virtual id_t add_listener(std::shared_ptr<io_listener> l) {
      std::lock_guard<std::shared_mutex> lg(_lock_listeners);
      auto id = _id.fetch_add(1);
      _listeners[id] = l;
      return id;
    }

    virtual id_t add_connection(std::shared_ptr<io_connection> c) {
      std::lock_guard<std::shared_mutex> lg(_lock_connections);
      auto id = _id.fetch_add(1);
      _connections[id] = c;
      return id;
    }

    virtual void rm_listener(id_t id) {
      std::lock_guard<std::shared_mutex> lg(_lock_listeners);
      _listeners.erase(id);
    }

    virtual void rm_connection(id_t id) {
      std::lock_guard<std::shared_mutex> lg(_lock_connections);
      _connections.erase(id);
    }

    void listeners_visit(std::function<bool(std::shared_ptr<io_listener>)> visitor) {
      std::shared_lock<std::shared_mutex> sl(_lock_listeners);
      for (auto v : _listeners) {
        auto continueFlag = visitor(v.second);
        if (!continueFlag) {
          break;
        }
      }
    }

    void connections_visit(std::function<bool(std::shared_ptr<io_connection>)> visitor) {
      std::shared_lock<std::shared_mutex> sl(_lock_connections);
      for (auto v : _connections) {
        auto continueFlag = visitor(v.second);
        if (!continueFlag) {
          break;
        }
      }
    }

    std::shared_ptr<io_connection> get_connection(const id_t id) {
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

    virtual bool is_stopped() const { return _stop_io_service; }

  private:
    params _params;
    mutable std::shared_mutex _lock_listeners;
    std::unordered_map<id_t, std::shared_ptr<io_listener>> _listeners;
    mutable std::shared_mutex _lock_connections;
    std::unordered_map<id_t, std::shared_ptr<io_connection>> _connections;

    std::atomic_uint64_t _id;

    boost::asio::io_service _io_service;
    std::vector<std::thread> _threads;
    bool _stop_io_service = false;
  };

  class io_listener : public base_io_chanel,
                      public std::enable_shared_from_this<io_listener>,
                     public utils ::waitable {
  public:
    io_listener(std::shared_ptr<io_manager> manager) : _manager(manager) {}
    virtual ~io_listener() { stop_listener(); }

    id_t get_id() const { return _id; }
    std::shared_ptr<io_manager> getmanager() const { return _manager; }

    virtual void on_error(const sender &i, const ecode &err) = 0;
    virtual void on_message(const sender &i, const Arg &&d) = 0;
    /**
    result - true for accept, false for failed.
    */
    virtual bool on_client(const sender &i) = 0;
    virtual void on_clientDisconnect(const sender &i) { UNUSED(i); };
    virtual async_operation_handler send_async(nmq::id_t client, const Result message) = 0;

    virtual void start_listener() { _id = _manager->add_listener(shared_from_this()); }

    virtual void stop_listener() {
      if (!is_stoped()) {
        _manager->rm_listener(_id);
      }
    }

  private:
    id_t _id;
    std::shared_ptr<io_manager> _manager; // TODO use std::weak_ptr?
  };

  class io_connection : public base_io_chanel,
                       public std::enable_shared_from_this<io_connection>,
                       public utils ::waitable {
  public:
    io_connection() = delete;
    io_connection(std::shared_ptr<io_manager> manager) : _manager(manager) {}
    virtual ~io_connection() { stop_connection(); }

    id_t get_id() const { return _id; }
    std::shared_ptr<io_manager> getmanager() const { return _manager; }

    virtual void on_connected() { start_complete(); }
    virtual void on_error(const ecode &err) { UNUSED(err); };
    virtual void on_message(const result_t &&d) = 0;
    virtual async_operation_handler send_async(const Arg message) = 0;

    virtual void start_connection() { _id = _manager->add_connection(shared_from_this()); }

    virtual void stop_connection() {
      if (!is_stoped()) {
        _manager->rm_connection(_id);
      }
    }

  private:
    id_t _id;
    std::shared_ptr<io_manager> _manager;
  };

  base_io_chanel() { _next_message_id = 0; }

  virtual void start() = 0;
  virtual void stop() = 0;

  uint64_t get_next_message_id() { return _next_message_id.fetch_add(1); }

  std::atomic_uint64_t _next_message_id;
};
} // namespace nmq