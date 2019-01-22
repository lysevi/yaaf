#include <libnmq/lockfree/queue.h>

#include "helpers.h"
#include <catch.hpp>

TEST_CASE("lockfree.queue") {
  const size_t test_queue_cap = 3;
  nmq::lockfree::FixedQueue<int> q{test_queue_cap};
  EXPECT_EQ(q.capacity(), test_queue_cap);

  EXPECT_TRUE(q.empty());

  EXPECT_TRUE(q.tryPush(1));
  EXPECT_TRUE(q.tryPush(2));
  EXPECT_TRUE(q.tryPush(3));
  EXPECT_FALSE(q.tryPush(1));

  auto r = q.tryPop();
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.result, 3);

  r = q.tryPop();
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.result, 2);

  r = q.tryPop();
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.result, 1);

  r = q.tryPop();
  EXPECT_FALSE(r.ok);
}
