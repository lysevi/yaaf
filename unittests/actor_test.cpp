#include <libnmq/context.h>

#include "helpers.h"
#include <catch.hpp>

TEST_CASE("actor") {
  int summ = 0;

  auto clbk = [&summ](nmq::envelope el) {

    int v = boost::any_cast<int>(el.payload);
    summ += v;
  };

  nmq::actor_ptr actor =
      std::make_shared<nmq::actor_for_delegate>(nullptr, nmq::actor_for_delegate::delegate_t(clbk));

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
