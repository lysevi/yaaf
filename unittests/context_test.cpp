#include "helpers.h"
#include <libnmq/context.h>
#include <libnmq/utils/logger.h>
#include <catch.hpp>

using namespace nmq;
using namespace nmq::utils::logging;

TEST_CASE("context") {
  auto ctx = std::make_shared<context>(context::params_t::defparams());
  int summ = 0;
  auto c1 = [&summ](nmq::actor_weak, nmq::envelope e) {
    auto v = boost::any_cast<int>(e.payload);
    summ += v;
  };
  auto c1_addr = ctx->add_actor(actor::delegate_t(c1));

  auto c2 = [](nmq::actor_weak, nmq::envelope) {};
  auto c2_addr = ctx->add_actor(actor::delegate_t(c2));
  EXPECT_NE(c1_addr.get_id(), c2_addr.get_id());

  c1_addr.send(int(1));
  c1_addr.send(int(2));
  c1_addr.send(int(3));
  c1_addr.send(std::string("wrong type"));
  c1_addr.send(int(4));

  while (summ != int(1 + 2 + 3 + 4)) {
    logger_info("summ!=1+2+3+4");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  c1_addr.stop();
  c1_addr.send(int(1));
}
