#include <libyaaf/context.h>

#include "helpers.h"
#include <catch.hpp>

#if YAAF_NETWORK_ENABLED

TEST_CASE("context. network", "[network][context]") {
  auto cp_listener = yaaf::context::params_t::defparams();
  auto cp_connection = yaaf::context::params_t::defparams();
  
  unsigned short listeners_count = 1;
  
  SECTION("context. network. 1 listener") { listeners_count = 1; }

  unsigned short started_port = 8080;
  for (unsigned short i = 0; i < listeners_count; ++i) {
    cp_listener.listeners_params.emplace_back(
        yaaf::network::listener::params_t{static_cast<unsigned short>(started_port + i)});

    cp_connection.connection_params.emplace_back(
        yaaf::network::connection::params_t("localhost", started_port + i));
  }

  auto ctx_lst = yaaf::context::make_context(cp_listener, "listen_context");
  auto ctx_con = yaaf::context::make_context(cp_connection, "con_context");
}
#endif