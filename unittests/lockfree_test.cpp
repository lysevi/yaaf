#include <libnmq/lockfree/queue.h>

#include "helpers.h"
#include <catch.hpp>

TEST_CASE("lockfree.queue") {
  const size_t test_queue_cap = 3;
  nmq::lockfree::FixedQueue<int> q{test_queue_cap};
  EXPECT_EQ(q.capacity(), test_queue_cap);

  int calls1 = 0;
  int calls2 = 0;
  auto clbk = [&calls1]() { calls1++; };
  auto clbk2 = [&calls2]() { calls2++; };

  EXPECT_TRUE(q.tryAddCallback(clbk));
  EXPECT_TRUE(q.tryAddCallback(clbk2));

  EXPECT_TRUE(q.empty());

  EXPECT_TRUE(q.tryPush(1));
  EXPECT_TRUE(q.tryPush(2));
  EXPECT_TRUE(q.tryPush(3));
  EXPECT_FALSE(q.tryPush(1));

  EXPECT_EQ(calls1, 3);
  EXPECT_EQ(calls2, 3);

  auto r = q.tryPop();
  EXPECT_TRUE(std::get<0>(r));
  EXPECT_EQ(std::get<1>(r), 3);

  r = q.tryPop();
  EXPECT_TRUE(std::get<0>(r));
  EXPECT_EQ(std::get<1>(r), 2);

  r = q.tryPop();
  EXPECT_TRUE(std::get<0>(r));
  EXPECT_EQ(std::get<1>(r), 1);

  r = q.tryPop();
  EXPECT_FALSE(std::get<0>(r));
}
