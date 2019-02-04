#include <libnmq/local/queue.h>

#include "helpers.h"
#include <catch.hpp>

TEST_CASE("local.queue") {
  const size_t test_queue_cap = 3;
  nmq::local::queue<int> q{test_queue_cap};

  EXPECT_TRUE(q.empty());

  EXPECT_TRUE(q.try_push(1));
  EXPECT_TRUE(q.try_push(2));
  EXPECT_TRUE(q.try_push(3));

  auto r = q.try_pop();
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.value, 3);

  r = q.try_pop();
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.value, 2);

  r = q.try_pop();
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.value, 1);

  r = q.try_pop();
  EXPECT_FALSE(r.ok);
}
