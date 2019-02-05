#include <libnmq/actor.h>
#include <libnmq/envelope.h>
#include <libnmq/mailbox.h>

#include "helpers.h"
#include <catch.hpp>

TEST_CASE("actor") {
  int summ = 0;

  auto clbk = [&summ](nmq::actor_weak a, nmq::envelope el) {
    if (auto sl = a.lock()) {
      EXPECT_TRUE(sl->busy());
    }
    int v = boost::any_cast<int>(el.payload);
    summ += v;
  };

  nmq::actor_ptr actor =
      std::make_shared<nmq::actor>(nullptr, nmq::actor::delegate_t(clbk));

  nmq::mailbox mbox;
  actor->apply(mbox);

  mbox.push(int(1), actor);
  mbox.push(int(2), actor);
  mbox.push(int(3), actor);
  mbox.push(int(4), actor);

  while (!mbox.empty()) {
    EXPECT_TRUE(actor->try_lock());
    actor->apply(mbox);
    EXPECT_FALSE(actor->busy());

    auto st = actor->status();
    EXPECT_EQ(st.kind, nmq::actor_status_kinds::NORMAL);
    EXPECT_EQ(st.msg, std::string());
  }

  EXPECT_EQ(summ, int(1) + 2 + 3 + 4);

  mbox.push(std::string("bad cast check"), actor);
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

  mbox.push(int(4), actor);
  EXPECT_TRUE(actor->try_lock());
  actor->apply(mbox);

  st = actor->status();
  EXPECT_EQ(st.kind, nmq::actor_status_kinds::NORMAL);
  EXPECT_EQ(st.msg, std::string());
}
