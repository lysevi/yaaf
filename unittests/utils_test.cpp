#include <libyaaf/utils/async/thread_manager.h>
#include <libyaaf/utils/async/thread_pool.h>

#include <libyaaf/utils/strings.h>
#include <libyaaf/utils/utils.h>

#include "helpers.h"
#include <catch.hpp>

#include <numeric>

TEST_CASE("utils.split") {

  std::array<int, 8> tst_a;
  std::iota(tst_a.begin(), tst_a.end(), 1);

  std::string str = "1 2 3 4 5 6 7 8";
  auto splitted_s = yaaf::utils::strings::tokens(str);

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
  auto res = yaaf::utils::strings::to_upper(s);
  EXPECT_EQ(res, "LOWER STRING");
}

TEST_CASE("utils.to_lower") {
  auto s = "UPPER STRING";
  auto res = yaaf::utils::strings::to_lower(s);
  EXPECT_EQ(res, "upper string");
}

TEST_CASE("utils.threads_pool") {
  using namespace yaaf::utils::async;

  const thread_kind_t tk = 1;
  {
    const size_t threads_count = 2;
    threads_pool tp(threads_pool::params_t(threads_count, tk));

    EXPECT_EQ(tp.threads_count(), threads_count);
    EXPECT_TRUE(!tp.is_stopped());
    tp.stop();
    EXPECT_TRUE(tp.is_stopped());
  }

  {
    const size_t threads_count = 2;
    threads_pool tp(threads_pool::params_t(threads_count, tk));
    const size_t tasks_count = 100;
    task at = [tk](const thread_info &ti) {
      if (tk != ti.kind) {
        INFO("(tk != ti.kind)");
        throw MAKE_EXCEPTION("(tk != ti.kind)");
      }
      return CONTINUATION_STRATEGY::SINGLE;
    };
    for (size_t i = 0; i < tasks_count; ++i) {
      tp.post(wrap_task(at));
    }
    tp.flush();

    auto lock = tp.post(wrap_task(at));
    lock->wait();

    tp.stop();
  }

  { // without flush
    const size_t threads_count = 2;
    threads_pool tp(threads_pool::params_t(threads_count, tk));
    const size_t tasks_count = 100;
    task at = [tk](const thread_info &ti) {
      if (tk != ti.kind) {
        INFO("(tk != ti.kind)");
        throw MAKE_EXCEPTION("(tk != ti.kind)");
      }
      return CONTINUATION_STRATEGY::SINGLE;
    };
    for (size_t i = 0; i < tasks_count; ++i) {
      tp.post(wrap_task(at));
    }

    tp.stop();
  }
}

TEST_CASE("utils.threads_manager") {
  using namespace yaaf::utils::async;

  const thread_kind_t tk1 = 1;
  const thread_kind_t tk2 = 2;
  size_t threads_count = 2;
  threads_pool::params_t tp1(threads_count, tk1);
  threads_pool::params_t tp2(threads_count, tk2);

  thread_manager::params_t tpm_params(std::vector<threads_pool::params_t>{tp1, tp2});
  {
    const size_t tasks_count = 10;

    thread_manager t_manager(tpm_params);
    int called = 0;
    uint64_t inf_calls = 0;
    task infinite_worker = [&inf_calls](const thread_info &) {
      ++inf_calls;
      return CONTINUATION_STRATEGY::REPEAT;
    };

    task at_while = [&called](const thread_info &) {
      if (called < 10) {
        ++called;
        return CONTINUATION_STRATEGY::REPEAT;
      }
      return CONTINUATION_STRATEGY::SINGLE;
    };
    task at1 = [tk1](const thread_info &ti) {
      if (tk1 != ti.kind) {
        INFO("(tk != ti.kind)");
        yaaf::utils::sleep_mls(400);
        throw MAKE_EXCEPTION("(tk1 != ti.kind)");
      }
      return CONTINUATION_STRATEGY::SINGLE;
    };
    task at2 = [tk2](const thread_info &ti) {
      if (tk2 != ti.kind) {
        INFO("(tk != ti.kind)");
        yaaf::utils::sleep_mls(400);
        throw MAKE_EXCEPTION("(tk2 != ti.kind)");
      }
      return CONTINUATION_STRATEGY::SINGLE;
    };
    t_manager.post(tk1, wrap_task_with_priority(infinite_worker, yaaf::utils::async::TASK_PRIORITY::WORKER));
    auto at_while_res = t_manager.post(tk1, wrap_task(at_while));
    for (size_t i = 0; i < tasks_count; ++i) {
      t_manager.post(tk1, wrap_task(at1));
      t_manager.post(tk2, wrap_task(at2));
    }
    // EXPECT_GT(ThreadManager::instance()->active_works(), size_t(0));
    at_while_res->wait();
    EXPECT_EQ(called, int(10));
    t_manager.flush();
  }
}
