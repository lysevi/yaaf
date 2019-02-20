#include <libyaaf/context.h>
#include <libyaaf/utils/logger.h>
#include <iostream>
#include <numeric>

const std::vector<uint8_t> tst_net_data = {0, 1, 2, 3, 4, 5, 6};
const unsigned short port_number = 9080;

class listener_actor : public yaaf::base_actor {
public:
  listener_actor() {}

  yaaf::actor_settings on_init(const yaaf::actor_settings &bs) override {
    return yaaf::base_actor::on_init(bs);
  }
  void on_start() override {
    started = true;
    yaaf::base_actor::on_start();
  }
  void on_stop() override { yaaf::base_actor::on_stop(); }
  void action_handle(const yaaf::envelope &e) override {
    auto v = e.payload.cast<yaaf::listener_actor_message>();
    sum_ = std::accumulate(v.data.begin(), v.data.end(), sum_);

    auto ctx = get_context();
    if (ctx != nullptr) {
      yaaf::listener_actor_message nam;
      nam.sender_id = v.sender_id;
      nam.name = "/root/usr/connection_actor";
      nam.data = v.data;
      ctx->send(e.sender, nam);
    }
  }

  bool started = false;
  unsigned char sum_ = (unsigned char)0;
};

class connection_actor : public yaaf::base_actor {
public:
  connection_actor() {}

  void on_start() override {
    started = true;
    yaaf::base_actor::on_start();
  }

  void action_handle(const yaaf::envelope &e) override {
    auto v = e.payload.cast<yaaf::network_actor_message>();
    sum_ = std::accumulate(v.data.begin(), v.data.end(), sum_);
  }

  bool started = false;
  unsigned char sum_ = 0;
};

class connection_started_actor : public yaaf::base_actor {
public:
  void on_start() override {
    auto ctx = get_context();
    if (ctx != nullptr) {
      ctx->subscribe_to_exchange("/root/net/localhost:9080");
    }
    started = true;
  }

  void action_handle(const yaaf::envelope &e) override {
    auto status = e.payload.cast<yaaf::connection_status_message>();

    if (status.is_connected) {
      auto ctx = get_context();
      if (ctx != nullptr) {
        auto con_actor_addr = ctx->get_address("/root/net/localhost:9080");

        yaaf::network_actor_message nmessage;
        nmessage.data = tst_net_data;
        nmessage.name = "/root/usr/listener_actor";
        ctx->send(con_actor_addr, nmessage);
      }
    }
  }

  bool started = false;
};

void init_connection(std::shared_ptr<yaaf::context> &ctx_con) {

  auto init_addr = ctx_con->make_actor<connection_started_actor>("test_initiator_actor");
  auto init_actor_ptr = dynamic_cast<connection_started_actor *>(
      ctx_con->get_actor(init_addr).lock().get());

  while (!init_actor_ptr->started) {
    yaaf::utils::logging::logger_info("test: wait !init_actor_ptr->started");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  auto con_actor_addr_a = ctx_con->make_actor<connection_actor>("connection_actor");
  auto con_actor_ptr = ctx_con->actor_cast<connection_actor>(con_actor_addr_a);

  while (!con_actor_ptr->started) {
    yaaf::utils::logging::logger_info("test: wait !testable_con_actor_ptr->started");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void init_listener(std::shared_ptr<yaaf::context> &ctx_lst) {
  auto testable_actor_addr_a = ctx_lst->make_actor<listener_actor>("listener_actor");
  auto testable_actor_ptr = ctx_lst->actor_cast<listener_actor>(testable_actor_addr_a);

  while (!testable_actor_ptr->started) {
    yaaf::utils::logging::logger_info("test: wait !lst_actor_ptr->started");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

int main(int, char **) {

  auto _raw_logger_ptr = new yaaf::utils::logging::quiet_logger();
  auto _logger = yaaf::utils::logging::abstract_logger_ptr{_raw_logger_ptr};
  yaaf::utils::logging::logger_manager::start(_logger);

  auto listener_params = yaaf::context::params_t::defparams();
  auto connection_params = yaaf::context::params_t::defparams();

  auto ctx_con = yaaf::context::make_context("con_context");
  ctx_con->send(ctx_con->get_address("/root/net"),
                dialler::dial::params_t("localhost", port_number));

  auto ctx_lst = yaaf::context::make_context(listener_params, "listen_context");
  ctx_lst->send(ctx_con->get_address("/root/net"),
                dialler::listener::params_t{static_cast<unsigned short>(port_number)});

  /// connection
  init_connection(ctx_con);
  /// listener
  init_listener(ctx_lst);

  auto target_summ =
      std::accumulate(tst_net_data.begin(), tst_net_data.end(), uint8_t(0));

  auto con_a = ctx_con->get_actor("/root/usr/connection_actor").lock();
  auto testable_con_actor_ptr = dynamic_cast<connection_actor *>(con_a.get());

  while (testable_con_actor_ptr->sum_ != target_summ) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}