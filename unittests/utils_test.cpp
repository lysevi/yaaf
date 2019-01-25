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

TEST_CASE("utils.longProcess") {
  nmq::utils::LongProcess run(std::string("run"), true);
  EXPECT_FALSE(run.isStarted());
  EXPECT_FALSE(run.isComplete());

  REQUIRE_THROWS(run.complete());

  run.start();
  EXPECT_TRUE(run.isStarted());
  EXPECT_FALSE(run.isComplete());

  run.complete();
  EXPECT_TRUE(run.isStarted());
  EXPECT_TRUE(run.isComplete());

  REQUIRE_THROWS(run.complete(true));
}

TEST_CASE("utils.waitable") {
  nmq::utils::Waitable child_w, parent_w;
  {
    EXPECT_FALSE(child_w.isStartBegin());

    parent_w.startBegin();
    child_w.startBegin();

    REQUIRE_THROWS(child_w.startBegin());
    REQUIRE_THROWS(parent_w.startBegin());

    EXPECT_TRUE(child_w.isStartBegin());

    auto chld = [&child_w, &parent_w]() {
      parent_w.waitStarting();
      EXPECT_TRUE(parent_w.isStarted());
      child_w.startComplete();
    };

    std::thread chldT(chld);

    parent_w.startComplete();
    child_w.waitStarting();
    EXPECT_TRUE(child_w.isStarted());

    chldT.join();
  }

  child_w.stopBegin();
  parent_w.stopBegin();
  auto chld_stoper = [&parent_w, &child_w]() {
    child_w.stopComplete();
    EXPECT_TRUE(child_w.isStoped());
    EXPECT_FALSE(child_w.isStarted());
    parent_w.waitStoping();
    EXPECT_TRUE(parent_w.isStoped());
    EXPECT_FALSE(parent_w.isStarted());
  };
  
  std::thread chldStoper(chld_stoper);

  child_w.waitStoping();

  
  parent_w.stopComplete();
  chldStoper.join();
}