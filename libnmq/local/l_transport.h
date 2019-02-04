#pragma once

#include <libnmq/chanel.h>
#include <libnmq/local/queue.h>
#include <boost/asio.hpp>

namespace nmq {
namespace local {

using boost::asio::io_service;

template <typename Arg, typename Result, class ArgQueue = queue<std::pair<id_t, Arg>>,
          class ResultQueue = queue<Result>>
struct transport {
  using self_t = transport<Arg, Result>;
  using arg_t = Arg;
  using result_t = Result;
  using io_chanel_t = base_io_chanel<Arg, Result>;
  using sender_t = typename io_chanel_t::sender_t;

  struct params_t : public io_chanel_t::params_t {
    params_t() {
      arg_queue_size = 10;
      result_queue_size = 10;
    }
    size_t arg_queue_size;
    size_t result_queue_size;
  };
  class connection;
  class listener;

  class manager : public io_chanel_t::io_manager, public ao_supervisor {
  public:
    using io_chanel_t::io_manager::shared_from_this;

    manager(const params_t &p)
        : io_chanel_t::io_manager(io_chanel_t::params_t(p.threads_count)), _params(p),
          _args(p.arg_queue_size) {
      ENSURE(_params.threads_count > 0);
      ENSURE(_params.arg_queue_size > 0);
      ENSURE(_params.result_queue_size > 0);
    }

    std::shared_ptr<manager> shared_self() {
      auto self = shared_from_this();
      return std::dynamic_pointer_cast<manager>(self);
    }

    id_t add_listener(std::shared_ptr<typename io_chanel_t::io_listener> l) override {
      auto res = io_chanel_t::io_manager::add_listener(l);
      return res;
    }

    id_t add_connection(std::shared_ptr<typename io_chanel_t::io_connection> c) override {
      auto res = io_chanel_t::io_manager::add_connection(c);
      return res;
    }

    void rm_listener(id_t id) override {
      io_chanel_t::io_manager::rm_listener(id);

      if (listeners_count() == 0) {
        connections_visit([](std::shared_ptr<io_chanel_t::io_connection> c) {
          c->on_error(ecode(errors_kinds::ALL_LISTENERS_STOPED));
          return true;
        });
      }
    }

    void rm_connection(id_t id) override { io_chanel_t::io_manager::rm_connection(id); }

    void start() override {
      initialisation_begin();
      io_chanel_t::io_manager::start();
      auto self = shared_self();
      post([self]() { self.get()->queue_worker(); });
      initialisation_complete();
    }

    void stop() override {
      stopping_started();
      connections_visit([](std::shared_ptr<io_chanel_t::io_connection> c) {
        c->on_error(ecode(errors_kinds::FULL_STOP));
        return true;
      });

      wait_all_async_operations();

      io_chanel_t::io_manager::stop();
      stopping_completed();
    }

    void push_arg_loop(id_t id, const Arg a, id_t aor) {
      if (is_stopping_started() || _args.try_push(std::make_pair(id, a))) {
        this->mark_operation_as_finished(aor);
        return;
      } else {
        auto self = shared_self();
        post([self, id, a, aor]() { self.get()->push_arg_loop(id, a, aor); });
      }
    }

    void push_to_result_loop(const id_t id, const Result a, id_t aor);

    void push_arg(id_t id, const Arg a, id_t aor) {
      auto self = shared_self();
      post([self, id, a, aor]() { self->push_arg_loop(id, a, aor); });
    }

    void pushResult(const nmq::id_t id, const Result a, id_t aor) {
      auto self = shared_self();
      post([self, id, a, aor]() { self->push_to_result_loop(id, a, aor); });
    }

    void queue_worker() {
      auto self = shared_self();

      if (!_args.empty()) {

        listeners_visit([self](std::shared_ptr<io_chanel_t::io_listener> l) {
          auto tptr = std::dynamic_pointer_cast<typename transport::listener>(l);

          if (!tptr->isBusy()) {
            return tptr->run(self->_args);
          }
          return true; // break visitors' loop
        });
      }

      if (!is_stopping_started()) {
        post([self]() { self->queue_worker(); });
      }
    }

  private:
    params_t _params;

    ArgQueue _args;
  };

  class listener : public io_chanel_t::io_listener {
  public:
    using io_chanel_t::io_listener::is_started;
    using io_chanel_t::io_listener::is_stoped;
    using io_chanel_t::io_listener::initialisation_begin;
    using io_chanel_t::io_listener::initialisation_complete;
    using io_chanel_t::io_listener::stopping_started;
    using io_chanel_t::io_listener::stopping_completed;

    listener() = delete;
    listener(const listener &) = delete;
    listener &operator=(const listener &) = delete;

    listener(std::shared_ptr<manager> manager,
             const transport::params_t & /*transport_params*/)
        : io_chanel_t::io_listener(manager) {
      _manager = manager;
    }

    bool on_client(const sender_t &) override { return true; }

    async_operation_handler send_async(const nmq::id_t id, const Result message) override {
      auto r = _manager->make_async_result();
      _manager->pushResult(id, message, r.id);
      return r;
    }

    void start_listener() override {
      initialisation_begin();
      io_chanel_t::io_listener::start_listener();
      initialisation_complete();
    }

    void stop_listener() override {
      stopping_started();
      io_chanel_t::io_listener::stop_listener();
      stopping_completed();
    }

    void start() { start_listener(); }

    void stop() { stop_listener(); }

    bool isBusy() { return _is_busy; }

    bool run(ArgQueue &args) {
      if (!_is_busy) { // TODO make thread safety
        auto a = args.try_pop();
        if (a.ok) {
          std::pair<id_t, Arg> arg = a.value;

          sender_t s{*this, arg.first};
          _is_busy = true;
          // TODO run on _manager->post
          on_message(s, std::move(arg.second));
          _is_busy = false;
          return true;
        }
      }
      return false;
    }

  private:
    std::shared_ptr<manager> _manager;
    bool _is_busy = false;
  };

  class connection : public io_chanel_t::io_connection {
  public:
    using io_chanel_t::io_listener::is_started;
    using io_chanel_t::io_listener::is_stoped;
    using io_chanel_t::io_listener::initialisation_begin;
    using io_chanel_t::io_listener::initialisation_complete;
    using io_chanel_t::io_listener::stopping_started;
    using io_chanel_t::io_listener::stopping_completed;

    connection() = delete;
    connection(const connection &) = delete;
    connection &operator=(const connection &) = delete;

    connection(std::shared_ptr<manager> manager, const transport::params_t &p)
        : _results(p.result_queue_size), io_chanel_t::io_connection(manager) {
      _manager = manager;
    }

    std::shared_ptr<connection> shared_self() {
      auto self = shared_from_this();
      return std::dynamic_pointer_cast<connection>(self);
    }

    void on_error(const ecode &err) override { io_chanel_t::io_connection::on_error(err); }

    void on_connected() override { io_chanel_t::io_connection::on_connected(); }

    async_operation_handler send_async(const Arg message) override {
      auto r = _manager->make_async_result();
      _manager->push_arg(get_id(), message, r.id);
      return r;
    }

    void start_connection() {
      initialisation_begin();
      io_chanel_t::io_connection::start_connection();
      auto self = shared_self();

      _manager->post([self]() { self->queue_worker(); });
    }

    void run(ResultQueue &q) {
      if (_is_busy.test_and_set(std::memory_order_acquire)) {
        auto d = q.try_pop();
        if (d.ok) {
          on_message(std::move(d.value));
        }
        _is_busy.clear(std::memory_order_release);
      }
      auto self = shared_self();
      _manager->post([self]() { self->queue_worker(); });
    }

    void stop_connection() { io_chanel_t::io_connection::stop_connection(); }

    void start() {
      start_connection();
      on_connected();
    }

    void stop() {
      stopping_started();
      stop_connection();
      stopping_completed();
    }

    friend manager;

  protected:
    void queue_worker() {
      auto self = shared_self();
      if (self->is_stopping_started()) {
        return;
      }
      if (!self->_results.empty()) {
        auto run = [self]() { self->run(self->_results); };
        self->_manager->post(run);
      } else {
        _manager->post([self]() { self->queue_worker(); });
      }
    }

  private:
    std::shared_ptr<manager> _manager;

    ResultQueue _results;
    std::atomic_flag _is_busy{ATOMIC_FLAG_INIT};
  };
};

template <class Arg, class Result, class ArgQueue, class ResultQueue>
void transport<Arg, Result, ArgQueue, ResultQueue>::manager::push_to_result_loop(
    const nmq::id_t id, const Result a, id_t aor) {
  auto target = get_connection(id);
  if (target == nullptr) { // TODO notify about it.
    this->mark_operation_as_finished(aor);
    return;
  }

  auto tptr = std::dynamic_pointer_cast<typename transport::connection>(target);
  ENSURE(tptr != nullptr);
  ENSURE(tptr->get_id() == id);
  if (tptr->is_stopping_started() || this->is_stopping_started() || tptr->_results.try_push(a)) {
    this->mark_operation_as_finished(aor);
    return;
  }
  auto self = shared_self();
  this->post([=]() {
    auto clbk = [=]() { self->push_to_result_loop(id, a, aor); };
    self->post(clbk);
  });
}

} // namespace local
} // namespace nmq