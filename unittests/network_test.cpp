#include <libyaaf/context.h>

#include "helpers.h"
#include <catch.hpp>

#if YAAF_NETWORK_ENABLED

TEST_CASE("context. network", "[network][context]") {
  auto cp_listener = yaaf::context::params_t::defparams();
  auto cp_connection = yaaf::context::params_t::defparams();

  unsigned short listeners_count = 1;

  SECTION("context. network. 1 listener") { listeners_count = 1; }
  SECTION("context. network. 2 listener") { listeners_count = 2; }
  SECTION("context. network. 10 listener") { listeners_count = 10; }

  unsigned short started_port = 8080;
  for (unsigned short i = 0; i < listeners_count; ++i) {
    cp_listener.listeners_params.emplace_back(
        yaaf::network::listener::params_t{static_cast<unsigned short>(started_port + i)});

    cp_connection.connection_params.emplace_back(
        yaaf::network::connection::params_t("localhost", started_port + i));
  }

  auto ctx_lst = yaaf::context::make_context(cp_listener, "listen_context");
  auto ctx_con = yaaf::context::make_context(cp_connection, "con_context");

  for (unsigned short i = 0; i < listeners_count; ++i) {
    auto port_str = std::to_string(started_port + i);
    auto con_actor = ctx_con->get_actor("/root/net/localhost:" + port_str);
    EXPECT_FALSE(con_actor.expired());

    auto lst_actor = ctx_lst->get_actor("/root/net/listen_" + port_str);
    EXPECT_FALSE(lst_actor.expired());
  }
}
#endif