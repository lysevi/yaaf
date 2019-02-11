#include <libyaaf/payload.h>

#include "helpers.h"
#include <catch.hpp>

TEST_CASE("payload") {
  yaaf::payload_t p;

  SECTION("payload. int") {
    yaaf::payload_t int_p(int(5));
    p = int_p;

    EXPECT_TRUE(!p.empty());
    EXPECT_TRUE(!int_p.empty());
    EXPECT_EQ(p.cast<int>(), int_p.cast<int>());
    EXPECT_EQ(p.cast<int>(), int(5));
    EXPECT_TRUE(p.is<int>());
    EXPECT_FALSE(p.is<std::string>());

    SECTION("payload. move") {
      yaaf::payload_t mv = std::move(p);
      EXPECT_TRUE(p.empty());
      EXPECT_EQ(mv.cast<int>(), int(5));
      EXPECT_TRUE(mv.is<int>());
    }
  }

  SECTION("payload. std::string") {
    yaaf::payload_t str_p(std::string("hello"));
    p = str_p;
    EXPECT_TRUE(!p.empty());
    EXPECT_TRUE(!str_p.empty());
    EXPECT_EQ(p.cast<std::string>(), str_p.cast<std::string>());
    EXPECT_EQ(p.cast<std::string>(), std::string("hello"));
    EXPECT_FALSE(p.is<int>());
    EXPECT_TRUE(p.is<std::string>());

    SECTION("payload. move") {
      yaaf::payload_t mv = std::move(p);
      EXPECT_TRUE(p.empty());
      EXPECT_EQ(mv.cast<std::string>(), std::string("hello"));
      EXPECT_TRUE(mv.is<std::string>());
    }
  }

  SECTION("payload. double") {
    yaaf::payload_t dbl(double(3.14));
    p = dbl;
    EXPECT_TRUE(!dbl.empty());
    EXPECT_EQ(p.cast<double>(), dbl.cast<double>());
    EXPECT_EQ(p.cast<double>(), double(3.14));
    EXPECT_FALSE(p.is<int>());
    EXPECT_TRUE(p.is<double>());

    SECTION("payload. move") {
      yaaf::payload_t mv = std::move(p);
      EXPECT_TRUE(p.empty());
      EXPECT_EQ(mv.cast<double>(), double(3.14));
      EXPECT_TRUE(mv.is<double>());
    }
  }
}
