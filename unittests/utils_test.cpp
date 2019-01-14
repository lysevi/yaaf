#include <libnmq/utils/strings.h>
#include <libnmq/utils/utils.h>

#include "helpers.h"
#include <catch.hpp>

TEST_CASE("utils.split") {
  std::string str = "1 2 3 4 5 6 7 8";
  auto splitted = nmq::utils::strings::tokens(str);
  EXPECT_EQ(splitted.size(), size_t(8));

  splitted = nmq::utils::strings::split(str, ' ');
  EXPECT_EQ(splitted.size(), size_t(8));
}

TEST_CASE("utils.to_upper") {
  auto s = "lower string";
  auto res = nmq::utils::strings::to_upper(s);
  EXPECT_EQ(res, "LOWER STRING");
}

TEST_CASE("utils.to_lower") {
  auto s = "UPPER STRING";
  auto res = nmq::utils::strings::to_lower(s);
  EXPECT_EQ(res, "upper string");
}
