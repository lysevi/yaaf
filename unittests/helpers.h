#pragma once

#include <string>

#define EXPECT_EQ(a, b) REQUIRE((a) == (b))
#define EXPECT_TRUE(a) REQUIRE((a))
#define EXPECT_FALSE(a) REQUIRE_FALSE(a)
#define EXPECT_DOUBLE_EQ(a, b) REQUIRE((a) == Approx((b)))
#define EXPECT_GT(a, b) REQUIRE((a) > (b))
#define EXPECT_LT(a, b) REQUIRE((a) < (b))
#define EXPECT_LE(a, b) REQUIRE((a) <= (b))
#define EXPECT_NE(a, b) REQUIRE((a) != (b))
#define EXPECT_GE(a, b) REQUIRE((a) >= (b))