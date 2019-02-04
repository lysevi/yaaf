#pragma once

#include <libnmq/chanel.h>
#include <libnmq/network/connection.h>
#include <libnmq/network/listener.h>
#include <libnmq/network/queries.h>
#include <libnmq/serialization/serialization.h>

namespace nmq {
namespace network {

template <typename Arg, typename Result> struct transport {
  using arg_t = Arg;
  using result_t = Result;

  using io_chanel_t = typename base_io_chanel<Arg, Result>;
  using sender_t = typename io_chanel_t::sender_t;
  using arg_scheme_t = serialization::object_packer<Arg>;
  using result_scheme_t = serialization::object_packer<Arg>;

  using net_listener_t = network::listener;
  using net_listener_consumer_t = network::abstract_listener_consumer;

  using net_connection_t = network::connection;
  using net_connection_consumer_t = network::abstract_connection_consumer;

  struct params_t : public io_chanel_t::params_t {
    params_t() { auto_reconnect = true; }
    std::string host;
    unsigned short port;
    bool auto_reconnect;
  };

  class manager : public io_chanel_t::io_manager {
  public:
    manager(const params_t &p)
        : _params(p), io_chanel_t::io_manager(io_chanel_t::params_t(p.threads_count)) {
    }

    void start() override {
      initialisation_begin();
      io_chanel_t::io_manager::start();
      initialisation_complete();
    }

    void stop() override {
      stopping_started();
      io_chanel_t::io_manager::stop();
      stopping_completed();
    }

  private:
    params_t _params;
  };

  class listener : public io_chanel_t::io_listener,
                   public net_listener_consumer_t,
                   public ao_supervisor {
  public:
    using io_chanel_t::io_listener::is_initialisation_begin;
    using io_chanel_t::io_listener::is_stopping_started;
    using io_chanel_t::io_listener::initialisation_begin;
    using io_chanel_t::io_listener::initialisation_complete;
    using io_chanel_t::io_listener::stopping_started;
    using io_chanel_t::io_listener::stopping_completed;
    using io_chanel_t::io_listener::wait_starting;
    using io_chanel_t::io_listener::wait_stoping;

    listener() = delete;
    listener(const listener &) = delete;
    listener &operator=(const listener &) = delete;

    listener(std::shared_ptr<manager> manager,
             const transport::params_t &transport_params_)
        : io_chanel_t::io_listener(manager), transport_params(transport_params_) {
      _next_message_id = 0;
      _manager = manager;
    }

    ~listener() {
      if (!is_stoped()) {
        stop();
      }
    }

    bool on_new_connection(listener_client_ptr i) override {
      return on_client(sender_t{*this, i->get_id()});
    }

    void on_disconnect(const listener_client_ptr &i) override {
      on_clientDisconnect(sender_t{*this, i->get_id()});
    }
    void on_network_error(listener_client_ptr i, const message_ptr &,
                        const boost::system::error_code &err) override {
      on_error(sender_t{*this, i->get_id()}, ecode{err});
    }

    void on_new_message(listener_client_ptr i, message_ptr &&d, bool &cancel) override {
      UNUSED(cancel);
      queries::packed_message<Arg> msg(std::move(d));
      // if (!is_stopping_started())
      auto okMsg = queries::ok(msg.asyncOperationid).get_message();
      send_to(i->get_id(), okMsg);

      on_message(sender_t{*this, i->get_id()}, std::move(msg.msg));
    }

    bool on_client(const sender_t &) override { return true; }

    async_operation_handler send_async(id_t client, const Result message) override {
      auto r = make_async_result();
      queries::packed_message<Result> msg(get_next_message_id(), r.id, client, message);

      auto nd = msg.get_message();
      send_to(client, nd);

      r.mark_as_finished();
      return r;
    }

    void start() override {
      std::lock_guard<std::mutex> lg(_locker);
      initialisation_begin();
      io_chanel_t::io_listener::start_listener();

      _lstnr = std::make_shared<net_listener_t>(getmanager()->service(),
                                             net_listener_t::params{transport_params.port});

      if (!is_listener_exists()) {
        _lstnr->add_consumer(this);
      }
      _lstnr->start();
      _lstnr->wait_starting();
      initialisation_complete();
    }

    void stop() override {
      std::lock_guard<std::mutex> lg(_locker);
      stopping_started();
      io_chanel_t::io_listener::stop_listener();
      _lstnr->stop();
      _lstnr->wait_stoping();
      _lstnr = nullptr;

      stopping_completed();
    }

    bool is_stoped() const { return io_chanel_t::io_listener::is_stoped(); }
    bool is_started() const { return io_chanel_t::io_listener::is_started(); }

  private:
    std::shared_ptr<net_listener_t> _lstnr;
    transport::params_t transport_params;
    std::mutex _locker;
    std::shared_ptr<manager> _manager;
  };

  class connection : public io_chanel_t::io_connection,
                     public net_connection_consumer_t,
                     public ao_supervisor {
  public:
    connection() = delete;
    connection(const connection &) = delete;
    connection &operator=(const connection &) = delete;

    connection(std::shared_ptr<manager> manager,
               const transport::params_t &transport_params)
        : _transport_params(transport_params), io_chanel_t::io_connection(manager) {
      _manager = manager;
    }

    void on_connect() override {
      io_chanel_t::io_connection::on_connected();
      this->on_connected();
    };

    void on_new_message(message_ptr &&d, bool &cancel) override {
      UNUSED(cancel);
      if (d->get_header()->kind == (network::message::kind_t)messagekinds::OK) {
        queries::ok okRes(std::move(d));
        mark_operation_as_finished(okRes.id);
      } else {
        auto self = shared_from_this();
        this->_manager->post([self, d]() {
          queries::packed_message<Result> msg(std::move(d));
          self->on_message(std::move(msg.msg));
        });
      }
    }

    void on_network_error(const message_ptr &,
                        const boost::system::error_code &err) override {

      on_error(ecode{err});
    }

    void on_error(const ecode &err) override {
      UNUSED(err);
      if (!is_stopping_started()) {
        stop();
      }
    }

    async_operation_handler send_async(const Arg message) override {
      auto r = make_async_result();
      queries::packed_message<Arg> msg(get_next_message_id(), r.id, get_id(), message);
      auto nd = msg.get_message();

      _connection->send_async(nd);

      return r;
    }

    void start() override {
      std::lock_guard<std::mutex> lg(_locker);
      initialisation_begin();

      net_connection_t::params nparams(_transport_params.host, _transport_params.port,
                                    _transport_params.auto_reconnect);
      _connection = std::make_shared<net_connection_t>(getmanager()->service(), nparams);

      io_connection::start_connection();
      if (!is_connection_exists()) {
        _connection->add_consumer(this);
      }

      _connection->start_async_connection();
    }

    void stop() override {
      std::lock_guard<std::mutex> lg(_locker);
      stopping_started();
      io_connection::stop_connection();
      _connection->disconnect();
      _connection->wait_stoping();
      _connection = nullptr;
      stopping_completed();
    }

    bool is_stoped() const { return io_chanel_t::io_connection::is_stoped(); }
    bool is_started() const { return io_chanel_t::io_connection::is_started(); }

  private:
    std::shared_ptr<net_connection_t> _connection;
    transport::params_t _transport_params;
    std::mutex _locker;

    std::shared_ptr<manager> _manager;
  };
};
} // namespace network
} // namespace nmq