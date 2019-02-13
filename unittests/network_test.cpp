#include <libyaaf/context.h>

#include "helpers.h"
#include <catch.hpp>

#if YAAF_NETWORK_ENABLED

TEST_CASE("context. network", "[network][context]") {
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
      auto v = e.payload.cast<yaaf::listener_message>();
      sum_ = std::accumulate(v.msg.data.begin(), v.msg.data.end(), sum_);

      auto ctx = get_context();
      if (ctx != nullptr) {
        v.msg.name = "/root/usr/testable_con_listener";
        ctx->send(e.sender, v);
      }
    }

    bool started = false;
    uint8_t sum_ = 0;
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

      /* auto ctx = get_context();
       if (ctx != nullptr) {

         ctx->send(e.sender, v);
       }*/
    }

    bool started = false;
    uint8_t sum_ = 0;
  };

  auto cp_listener = yaaf::context::params_t::defparams();
  auto cp_connection = yaaf::context::params_t::defparams();

  unsigned short listeners_count = 1;

  SECTION("context. network. 1 listener") { listeners_count = 1; }
  /* SECTION("context. network. 2 listener") { listeners_count = 2; }
   SECTION("context. network. 10 listener") { listeners_count = 10; }*/

  unsigned short started_port = 8080;
  for (unsigned short i = 0; i < listeners_count; ++i) {
    cp_listener.listeners_params.emplace_back(
        yaaf::network::listener::params_t{static_cast<unsigned short>(started_port + i)});

    cp_connection.connection_params.emplace_back(
        yaaf::network::connection::params_t("localhost", started_port + i));
  }

  auto ctx_lst = yaaf::context::make_context(cp_listener, "listen_context");
  auto testable_actor_addr_a = ctx_lst->make_actor<testable_actor>("testable_listener");
  auto testable_actor_ptr = dynamic_cast<testable_actor *>(
      ctx_lst->get_actor(testable_actor_addr_a).lock().get());

  while (!testable_actor_ptr->started) {
    yaaf::utils::logging::logger_info("test: wait !lst_actor_ptr->started");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  auto ctx_con = yaaf::context::make_context(cp_connection, "con_context");
  auto testable_con_actor_addr_a =
      ctx_con->make_actor<testable_con_actor>("testable_con_listener");
  auto testable_con_actor_ptr = dynamic_cast<testable_con_actor *>(
      ctx_con->get_actor(testable_con_actor_addr_a).lock().get());

  while (!testable_con_actor_ptr->started) {
    yaaf::utils::logging::logger_info("test: wait !testable_con_actor_ptr->started");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  for (unsigned short i = 0; i < listeners_count; ++i) {
    auto port_str = std::to_string(started_port + i);
    auto con_actor = ctx_con->get_actor("/root/net/localhost:" + port_str);
    EXPECT_FALSE(con_actor.expired());

    auto lst_actor = ctx_lst->get_actor("/root/net/listen_" + port_str);
    EXPECT_FALSE(lst_actor.expired());

    yaaf::network_actor_message nmessage;
    nmessage.data = std::vector<uint8_t>({0, 1, 2, 3, 4, 5, 6});
    nmessage.name = "/root/usr/testable_listener";
    ctx_con->send(con_actor.lock()->self_addr(), nmessage);

    auto target_summ =
        std::accumulate(nmessage.data.begin(), nmessage.data.end(), nmessage.data[0]);
    while (testable_actor_ptr->sum_ != target_summ) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    while (testable_con_actor_ptr->sum_ != target_summ) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}
#endif