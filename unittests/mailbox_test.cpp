#include <libyaaf/context.h>

#include "helpers.h"
#include <catch.hpp>

TEST_CASE("mailbox") {
  yaaf::mailbox mbox;
  EXPECT_TRUE(mbox.empty());

  mbox.push(std::string("svalue"), yaaf::actor_address());
  mbox.push(int(1), yaaf::actor_address());
  mbox.push(std::make_shared<std::string>("shared string"), yaaf::actor_address());

  EXPECT_FALSE(mbox.empty());
  EXPECT_EQ(mbox.size(), size_t(3));

  yaaf::envelope out_v;

  EXPECT_TRUE(mbox.try_pop(out_v));
  EXPECT_EQ(out_v.payload.cast<std::string>(), std::string("svalue"));
  EXPECT_TRUE(mbox.try_pop(out_v));
  EXPECT_EQ(out_v.payload.cast<int>(), int(1));
  EXPECT_TRUE(mbox.try_pop(out_v));

  std::shared_ptr<std::string> str_ptr =
      out_v.payload.cast<std::shared_ptr<std::string>>();
  EXPECT_EQ(*str_ptr, std::string("shared string"));

  EXPECT_FALSE(mbox.try_pop(out_v));
  EXPECT_TRUE(mbox.empty());
}
