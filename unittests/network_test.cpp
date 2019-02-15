#include <libyaaf/context.h>

#include "helpers.h"
#include <catch.hpp>

#if YAAF_NETWORK_ENABLED

namespace {
const std::vector<uint8_t> tst_net_data = {0, 1, 2, 3, 4, 5, 6};

class testable_actor : public yaaf::base_actor {
public:
  testable_actor() {}

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
      nam.name = "/root/usr/testable_con_listener";
      nam.data = v.data;
      ctx->send(e.sender, nam);
    }
  }

  bool started = false;
  unsigned char sum_ = (unsigned char)0;
};

class testable_con_actor : public yaaf::base_actor {
public:
  testable_con_actor() {}

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

class test_initiator_actor : public yaaf::base_actor {
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
        EXPECT_FALSE(con_actor_addr.empty());

        yaaf::network_actor_message nmessage;
        nmessage.data = tst_net_data;
        nmessage.name = "/root/usr/testable_listener";
        ctx->send(con_actor_addr, nmessage);
      }
    }
  }

  bool started = false;
};
} // namespace

TEST_CASE("context. network", "[network][context]") {

  auto cp_listener = yaaf::context::params_t::defparams();
  auto cp_connection = yaaf::context::params_t::defparams();

  unsigned short listeners_count = 1;

  SECTION("context. network. 1 listener") { listeners_count = 1; }

  unsigned short started_port = 9080;
  for (unsigned short i = 0; i < listeners_count; ++i) {
    cp_listener.listeners_params.emplace_back(
        yaaf::network::listener::params_t{static_cast<unsigned short>(started_port + i)});

    cp_connection.connection_params.emplace_back(
        yaaf::network::connection::params_t("localhost", started_port + i));
  }
  /// connection
  auto ctx_con = yaaf::context::make_context(cp_connection, "con_context");
  auto init_addr = ctx_con->make_actor<test_initiator_actor>("test_initiator_actor");
  auto init_actor_ptr = ctx_con->actor_cast<test_initiator_actor>(init_addr);

  while (!init_actor_ptr->started) {
    yaaf::utils::logging::logger_info("test: wait !init_actor_ptr->started");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  auto testable_con_actor_addr_a =
      ctx_con->make_actor<testable_con_actor>("testable_con_listener");
  auto testable_con_actor_ptr =
      ctx_con->actor_cast<testable_con_actor>(testable_con_actor_addr_a);

  while (!testable_con_actor_ptr->started) {
    yaaf::utils::logging::logger_info("test: wait !testable_con_actor_ptr->started");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  /// listener
  auto ctx_lst = yaaf::context::make_context(cp_listener, "listen_context");
  auto testable_actor_addr_a = ctx_lst->make_actor<testable_actor>("testable_listener");
  auto testable_actor_ptr = ctx_lst->actor_cast<testable_actor>(testable_actor_addr_a);

  while (!testable_actor_ptr->started) {
    yaaf::utils::logging::logger_info("test: wait !lst_actor_ptr->started");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  for (unsigned short i = 0; i < listeners_count; ++i) {
    auto port_str = std::to_string(started_port + i);
    auto con_actor_addr = ctx_con->get_address("/root/net/localhost:" + port_str);
    EXPECT_FALSE(con_actor_addr.empty());

    auto lst_actor = ctx_lst->get_actor("/root/net/listen_" + port_str);
    EXPECT_FALSE(lst_actor.expired());

    auto target_summ =
        std::accumulate(tst_net_data.begin(), tst_net_data.end(), uint8_t(0));

    while (testable_actor_ptr->sum_ != target_summ) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    while (testable_con_actor_ptr->sum_ != target_summ) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}
#endif