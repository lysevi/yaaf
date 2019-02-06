#include <libnmq/context.h>

#include "helpers.h"
#include <catch.hpp>

TEST_CASE("actor") {
  int summ = 0;

  auto clbk = [&summ](nmq::envelope el) {

    int v = boost::any_cast<int>(el.payload);
    summ += v;
  };

  nmq::actor_ptr actor = std::make_shared<nmq::actor_for_delegate>(
      nmq::actor_for_delegate::delegate_t(clbk));

  nmq::mailbox mbox;
  actor->apply(mbox);

  mbox.push(int(1), nmq::actor_address());
  mbox.push(int(2), nmq::actor_address());
  mbox.push(int(3), nmq::actor_address());
  mbox.push(int(4), nmq::actor_address());

  while (!mbox.empty()) {
    EXPECT_TRUE(actor->try_lock());
    actor->apply(mbox);
    EXPECT_FALSE(actor->busy());

    auto st = actor->status();
    EXPECT_EQ(st.kind, nmq::actor_status_kinds::NORMAL);
    EXPECT_EQ(st.msg, std::string());
  }

  EXPECT_EQ(summ, int(1) + 2 + 3 + 4);

  mbox.push(std::string("bad cast check"), nmq::actor_address());
  bool has_exception = false;
  try {
    EXPECT_TRUE(actor->try_lock());
    actor->apply(mbox);
  } catch (...) {
    has_exception = true;
  }

  EXPECT_TRUE(has_exception);
  EXPECT_FALSE(actor->busy());
  auto st = actor->status();
  EXPECT_EQ(st.kind, nmq::actor_status_kinds::WITH_ERROR);
  EXPECT_NE(st.msg, std::string());

  mbox.push(int(4), nmq::actor_address());
  EXPECT_TRUE(actor->try_lock());
  actor->apply(mbox);

  st = actor->status();
  EXPECT_EQ(st.kind, nmq::actor_status_kinds::NORMAL);
  EXPECT_EQ(st.msg, std::string());
}

TEST_CASE("actor.settings") {
  class testable_actor : public nmq::base_actor {
  public:
    nmq::actor_settings on_init() override {
      is_on_init_called = true;
      return nmq::base_actor::on_init();
    }
    void on_stop() override {
      is_on_stop_called = true;
      nmq::base_actor::on_stop();
    }

    void action_handle(nmq::envelope &) override {}

    bool is_on_init_called = false;
    bool is_on_stop_called = false;
  };

  nmq::actor_ptr aptr = std::make_shared<testable_actor>();
  nmq::context_ptr ctx =
      std::make_shared<nmq::context>(nmq::context::params_t::defparams());
  auto aptr_addr = ctx->add_actor(aptr);
  aptr_addr.stop();
  auto testable_a_ptr = dynamic_cast<testable_actor *>(aptr.get());
  EXPECT_TRUE(testable_a_ptr->is_on_init_called);
  EXPECT_TRUE(testable_a_ptr->is_on_stop_called);
}