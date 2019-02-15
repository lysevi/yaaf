#if YAAF_NETWORK_ENABLED

#include <libyaaf/actor.h>
#include <libyaaf/context.h>
#include <libyaaf/envelope.h>
#include <libyaaf/network/queries.h>

using namespace yaaf;
using namespace yaaf::utils::logging;

namespace {
class network_actor : public base_actor {
public:
  void action_handle(const envelope &e) { UNUSED(e); }
};

class network_lst_actor : public base_actor,
                          public yaaf::network::abstract_listener_consumer {
public:
  void action_handle(const envelope &e) {
    auto lm = e.payload.cast<listener_actor_message>();
    network_actor_message nam;
    nam.data = lm.data;
    nam.name = lm.name;
    yaaf::network::queries::packed_message<yaaf::network_actor_message> pm(nam);
    auto msg_ptr = pm.get_message();
    send_to(lm.sender_id, msg_ptr);
  }

  bool on_new_connection(yaaf::network::listener_client_ptr c) override { return true; }

  void on_network_error(yaaf::network::listener_client_ptr i,
                        const network::message_ptr & /*d*/,
                        const boost::system::error_code & /*err*/) override {}

  void on_new_message(yaaf::network::listener_client_ptr i, network::message_ptr &&d,
                      bool & /*cancel*/) override {
    if (d->get_header()->kind == (network::message::kind_t)network::messagekinds::MSG) {
      network::queries::packed_message<network_actor_message> nm(d);
      auto ctx = get_context();
      if (ctx != nullptr) {
        auto addr = ctx->get_address(nm.msg.name);
        if (!addr.empty()) {
          listener_actor_message lm;
          lm.name = nm.msg.name;
          lm.data = nm.msg.data;
          lm.sender_id = i->get_id().value;
          ctx->send(addr, lm);
        } else {
          logger_fatal("context: listener - cannot get actor ", nm.msg.name);
        }
      }
    }
  }

  void on_disconnect(const yaaf::network::listener_client_ptr & /*i*/) override {}
};

class network_con_actor : public base_actor,
                          public yaaf::network::abstract_connection_consumer {
public:
  std::string target_host;
  network_con_actor(std::shared_ptr<yaaf::network::connection> con_, std::string host) {
    _con = con_;
    target_host = host;
  }

  void action_handle(const envelope &e) {
    network_actor_message nm = e.payload.cast<network_actor_message>();
    yaaf::network::queries::packed_message<yaaf::network_actor_message> pm(nm);

    _con->send_async(pm.get_message());
  }

  void on_connect() override { send_status_success(); };

  void on_new_message(yaaf::network::message_ptr &&d, bool &quet) override {
    UNUSED(quet);
    if (d->get_header()->kind == (network::message::kind_t)network::messagekinds::MSG) {
      network::queries::packed_message<network_actor_message> nm(d);
      auto ctx = get_context();
      if (ctx != nullptr) {
        auto target_addr = ctx->get_address(nm.msg.name);
        if (!target_addr.empty()) {
          ctx->send(target_addr, nm.msg);
        } else {
          logger_fatal("context: connection - cannot get actor ", nm.msg.name);
        }
      }
    }
  }
  void on_network_error(const yaaf::network::message_ptr &,
                        const boost::system::error_code &err) override {
    send_status_error(err.message());
  }

  void send_status_error(const std::string &err) {
    send_status(connection_status_message{target_host, err, false});
  }

  void send_status_success() {
    send_status(connection_status_message{target_host, {}, true});
  }

  void send_status(const connection_status_message &sm) {
    auto ctx = get_context();
    if (ctx != nullptr) {
      ctx->publish("/root/net/" + target_host, sm);
    }
  }

private:
  std::shared_ptr<yaaf::network::connection> _con;
};

} // namespace
void context::network_init() {
  logger_info("context: network init...");

  _net_root = this->add_actor("net", _root, std::make_shared<network_actor>());

  if (_params.listeners_params.empty() && _params.connection_params.empty()) {
    logger_info("context: network params is empty.");
    return;
  }

  for (int i = 0; i < _params.network_threads; ++i) {
    _net_threads.emplace_back([this]() {
      while (!this->is_stopping_begin()) {
        this->_net_service.poll_one();
      }
    });
  }

  for (auto lp : _params.listeners_params) {
    logger_info("context: start listener on ", lp.port);
    auto l = std::make_shared<network::listener>(&this->_net_service, lp);
    auto saptr = std::make_shared<network_lst_actor>();
    auto lactor = this->add_actor("listen_" + std::to_string(lp.port), _net_root, saptr);

    l->add_consumer(saptr.get());
    l->start();
    l->wait_starting();
    _network_listeners.emplace_back(l);
  }

  for (auto lp : _params.connection_params) {
    auto target_host = utils::strings::args_to_string(lp.host, ":", lp.port);
    logger_info("context: connecting to ", target_host);
    create_exchange(_net_root, "/root/net/" + target_host);

    auto l = std::make_shared<network::connection>(&this->_net_service, lp);
    auto actor_name = lp.host + ':' + std::to_string(lp.port);
    auto saptr = std::make_shared<network_con_actor>(l, target_host);
    auto lactor = this->add_actor(actor_name, _net_root, saptr);

    l->add_consumer(saptr.get());
    l->start_async_connection();
    _network_connections.emplace_back(l);
  }
}

#endif