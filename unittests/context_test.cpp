#include "helpers.h"
#include <libnmq/context.h>
#include <libnmq/utils/logger.h>
#include <catch.hpp>

using namespace nmq;
using namespace nmq::utils::logging;

TEST_CASE("context. sending", "[context]") {
  auto ctx = nmq::context::make_context();
  int summ = 0;
  auto c1 = [&summ](nmq::envelope e) {
    auto v = boost::any_cast<int>(e.payload);
    summ += v;
  };

  auto c2 = [](nmq::envelope) {};
  auto c1_addr = ctx->add_actor(actor_for_delegate::delegate_t(c1));
  auto c2_addr = ctx->add_actor(actor_for_delegate::delegate_t(c2));

  EXPECT_NE(c1_addr.get_id(), c2_addr.get_id());

  SECTION("context. many values") {
    nmq::envelope e;
    e.sender = c2_addr;

    auto send_helper = [c1_addr, ctx](auto v) { ctx->send(c1_addr, v); };

    send_helper(int(1));
    send_helper(int(2));
    send_helper(int(3));
    send_helper(std::string("wrong type"));
    send_helper(int(4));

    while (summ != int(1 + 2 + 3 + 4)) {
      logger_info("summ!=1+2+3+4");
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  SECTION("context. to stopped actor") {
    ctx->stop_actor(c1_addr);
    ctx->send(c1_addr, nmq::envelope{int(1), c2_addr});
  }

  ctx = nullptr;
}

TEST_CASE("context. actor_start_stop", "[context]") {
  class testable_actor : public nmq::base_actor {
  public:
    testable_actor(int ctor_arg_) : ctor_arg(ctor_arg_) {}

    nmq::actor_settings on_init(const nmq::actor_settings &bs) override {
      is_on_init_called = true;
      return nmq::base_actor::on_init(bs);
    }
    void on_stop() override {
      is_on_stop_called = true;
      nmq::base_actor::on_stop();
    }

    void action_handle(const nmq::envelope &) override {}

    int ctor_arg;
    bool is_on_init_called = false;
    bool is_on_stop_called = false;
  };

  auto ctx = nmq::context::make_context();

  auto aptr_addr = ctx->make_actor<testable_actor>(int(1));
  nmq::actor_ptr aptr = ctx->get_actor(aptr_addr);

  ctx->stop_actor(aptr_addr);

  auto testable_a_ptr = dynamic_cast<testable_actor *>(aptr.get());

  SECTION("check start|stop flags") {
    EXPECT_TRUE(testable_a_ptr->is_on_init_called);
    EXPECT_TRUE(testable_a_ptr->is_on_stop_called);
  }
}