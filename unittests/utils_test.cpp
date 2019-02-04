#include <libnmq/utils/initialized_resource.h>
#include <libnmq/utils/strings.h>
#include <libnmq/utils/utils.h>

#include "helpers.h"
#include <catch.hpp>

#include <numeric>

TEST_CASE("utils.split") {

  std::array<int, 8> tst_a;
  std::iota(tst_a.begin(), tst_a.end(), 1);

  std::string str = "1 2 3 4 5 6 7 8";
  auto splitted_s = nmq::utils::strings::tokens(str);

  std::vector<int> splitted(splitted_s.size());
  std::transform(splitted_s.begin(), splitted_s.end(), splitted.begin(),
                 [](const std::string &s) { return std::stoi(s); });

  EXPECT_EQ(splitted.size(), size_t(8));

  bool is_equal =
      std::equal(tst_a.begin(), tst_a.end(), splitted.begin(), splitted.end());
  EXPECT_TRUE(is_equal);
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

TEST_CASE("utils.long_process") {
  nmq::utils::long_process run(std::string("run"), true);
  EXPECT_FALSE(run.is_started());
  EXPECT_FALSE(run.is_complete());

  REQUIRE_THROWS(run.complete());

  run.start();
  EXPECT_TRUE(run.is_started());
  EXPECT_FALSE(run.is_complete());

  run.complete();
  EXPECT_TRUE(run.is_started());
  EXPECT_TRUE(run.is_complete());

  REQUIRE_THROWS(run.complete(true));
}

TEST_CASE("utils.initialized_resource") {
  nmq::utils::initialized_resource child_w, parent_w;
  {
    EXPECT_FALSE(child_w.is_initialisation_begin());

    parent_w.initialisation_begin();
    child_w.initialisation_begin();

    REQUIRE_THROWS(child_w.initialisation_begin());
    REQUIRE_THROWS(parent_w.initialisation_begin());

    EXPECT_TRUE(child_w.is_initialisation_begin());

    auto chld = [&child_w, &parent_w]() {
      parent_w.wait_starting();
      EXPECT_TRUE(parent_w.is_started());
      child_w.initialisation_complete();
    };

    std::thread chldT(chld);

    parent_w.initialisation_complete();
    child_w.wait_starting();
    EXPECT_TRUE(child_w.is_started());

    chldT.join();
  }

  child_w.stopping_started();
  parent_w.stopping_started();
  auto chld_stoper = [&parent_w, &child_w]() {
    child_w.stopping_completed();
    EXPECT_TRUE(child_w.is_stoped());
    EXPECT_FALSE(child_w.is_started());
    parent_w.wait_stoping();
    EXPECT_TRUE(parent_w.is_stoped());
    EXPECT_FALSE(parent_w.is_started());
  };

  std::thread chldStoper(chld_stoper);

  child_w.wait_stoping();

  parent_w.stopping_completed();
  chldStoper.join();
}