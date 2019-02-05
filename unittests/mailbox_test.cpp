#include <libnmq/context.h>

#include "helpers.h"
#include <catch.hpp>

TEST_CASE("mailbox") {
  nmq::mailbox mbox;
  EXPECT_TRUE(mbox.empty());

  mbox.push(std::string("svalue"), nmq::actor_address());
  mbox.push(int(1), nmq::actor_address());
  mbox.push(std::make_shared<std::string>("shared string"), nmq::actor_address());

  EXPECT_FALSE(mbox.empty());
  EXPECT_EQ(mbox.size(), size_t(3));

  nmq::envelope out_v;

  EXPECT_TRUE(mbox.try_pop(out_v));
  EXPECT_EQ(boost::any_cast<std::string>(out_v.payload), std::string("svalue"));
  EXPECT_TRUE(mbox.try_pop(out_v));
  EXPECT_EQ(boost::any_cast<int>(out_v.payload), int(1));
  EXPECT_TRUE(mbox.try_pop(out_v));

  std::shared_ptr<std::string> str_ptr =
      boost::any_cast<std::shared_ptr<std::string>>(out_v.payload);
  EXPECT_EQ(*str_ptr, std::string("shared string"));

  EXPECT_FALSE(mbox.try_pop(out_v));
  EXPECT_TRUE(mbox.empty());
}
