#include "helpers.h"
#include <libyaaf/context.h>
#include <libyaaf/utils/logger.h>
#include <algorithm>

#include <catch.hpp>
#include <map>

using namespace yaaf;
using namespace yaaf::utils::logging;

TEST_CASE("context. name", "[context]") {
  auto ctx = yaaf::context::make_context();
  auto ctx2 = yaaf::context::make_context();

  EXPECT_NE(ctx->name(), ctx2->name());
  ctx = nullptr;
  ctx2 = nullptr;
}

TEST_CASE("context. sending", "[context]") {
  auto ctx = yaaf::context::make_context();
  int summ = 0;
  auto c1 = [&summ](yaaf::envelope e) {
    auto v = e.payload.cast<int>();
    summ += v;
  };

  auto c2 = [](yaaf::envelope) {};
  auto c1_addr = ctx->make_actor<yaaf::actor_for_delegate>("c1", c1);
  auto c2_addr = ctx->make_actor<yaaf::actor_for_delegate>("c2", c2);

  EXPECT_NE(c1_addr.get_pathname(), "null");
  EXPECT_NE(c2_addr.get_pathname(), "null");
  EXPECT_NE(c1_addr.get_id(), c2_addr.get_id());

  SECTION("context. many values") {
    yaaf::envelope e;
    e.sender = c2_addr;

    auto send_helper = [c1_addr, ctx](auto v) { ctx->send(c1_addr, v); };

    send_helper(int(1));
    send_helper(int(2));
    send_helper(int(3));
    send_helper(std::string("wrong type"));
    send_helper(int(4));

    while (summ != int(1 + 2 + 3 + 4)) {
      logger_info("summ!=1+2+3+4");
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  SECTION("context. to stopped actor") {
    ctx->stop_actor(c1_addr);
    ctx->send(c1_addr, yaaf::envelope{int(1), c2_addr});
  }

  ctx = nullptr;
}

TEST_CASE("context. actor_start_stop", "[context]") {
  class testable_actor : public yaaf::base_actor {
  public:
    testable_actor(int ctor_arg_) : ctor_arg(ctor_arg_) {}

    yaaf::actor_settings on_init(const yaaf::actor_settings &bs) override {
      auto ctx = get_context();
      if (ctx == nullptr) {
        throw std::logic_error("context is nullptr");
      }
      auto n = ctx->name();
      EXPECT_TRUE(n != std::string(""));

      is_on_init_called = true;
      return yaaf::base_actor::on_init(bs);
    }

    void on_start() override {
      is_on_start_called = true;
      yaaf::base_actor::on_start();
    }

    void on_stop() override {
      is_on_stop_called = true;
      yaaf::base_actor::on_stop();
    }

    void action_handle(const yaaf::envelope &) override {}

    int ctor_arg;
    bool is_on_init_called = false;
    bool is_on_start_called = false;
    bool is_on_stop_called = false;
  };

  auto ctx = yaaf::context::make_context();

  auto aptr_addr = ctx->make_actor<testable_actor>("testable", int(1));
  auto testable_a_ptr = ctx->actor_cast<testable_actor>(aptr_addr);

  while (!testable_a_ptr->is_on_init_called || !testable_a_ptr->is_on_start_called) {
    logger_info("wait while the testable_actor was not started...");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  EXPECT_TRUE(testable_a_ptr->status().kind == yaaf::actor_status_kinds::NORMAL);

  ctx->stop_actor(aptr_addr);

  EXPECT_TRUE(testable_a_ptr->is_on_init_called);
  EXPECT_TRUE(testable_a_ptr->is_on_start_called);
  EXPECT_TRUE(testable_a_ptr->is_on_stop_called);
  auto k = testable_a_ptr->status().kind;
  EXPECT_TRUE(k == yaaf::actor_status_kinds::STOPED);

  ctx = nullptr;
}

TEST_CASE("context. hierarchy initialize", "[context]") {

  class child1_a : public yaaf::base_actor {
  public:
    child1_a() {}

    yaaf::actor_settings on_init(const yaaf::actor_settings &bs) override {
      is_on_init_called = true;
      return yaaf::base_actor::on_init(bs);
    }

    void on_start() override { is_on_start_called = true; }

    void on_stop() override {
      is_on_stop_called = true;
      yaaf::base_actor::on_stop();
    }

    void action_handle(const yaaf::envelope &e) override { e.payload.cast<int>(); }

    bool is_on_init_called = false;
    bool is_on_stop_called = false;
    bool is_on_start_called = false;
  };

  class root_a : public yaaf::base_actor {

  public:
    root_a() {}

    yaaf::actor_action_when_error on_child_error(const actor_address &addr) {
      UNUSED(addr);
      auto it = children.find(addr);
      EXPECT_TRUE(it != children.end());
      return on_error_flag;
    }

    void on_child_stopped(const yaaf::actor_address &addr,
                          yaaf::actor_stopping_reason reason) override {
      stopped_childs_count++;
      stopped[addr.get_id()] = reason;
    }

    void on_child_status(const yaaf::actor_address &addr,
                         yaaf::actor_status_kinds k) override {
      statuses_count++;
      statuses[addr.get_id()] = k;
    }

    yaaf::actor_settings on_init(const yaaf::actor_settings &bs) override {
      yaaf::actor_settings result = bs;
      return yaaf::base_actor::on_init(result);
    }

    void on_start() override {
      auto ctx = get_context();

      if (ctx == nullptr) {
        throw std::logic_error("context is nullptr");
      }

      for (int i = 0; i < 3; ++i) {
        auto a = ctx->make_actor<child1_a>("child_" + std::to_string(i));
        children.insert(a);
      }

      is_on_start_called = true;
      yaaf::base_actor::on_start();
    }

    void on_stop() override {
      is_on_stop_called = true;
      yaaf::base_actor::on_stop();
    }

    void action_handle(const yaaf::envelope &) override {}

    bool is_on_start_called = false;
    bool is_on_stop_called = false;
    std::unordered_set<yaaf::actor_address> children;

    size_t stopped_childs_count = 0;
    std::map<yaaf::id_t, yaaf::actor_stopping_reason> stopped;

    size_t statuses_count = 0;
    std::map<yaaf::id_t, yaaf::actor_status_kinds> statuses;

    yaaf::actor_action_when_error on_error_flag;
  };

  class core_a : public yaaf::base_actor {
  public:
    void on_start() override {
      auto ctx = get_context();
      if (ctx != nullptr) {
        root_addr = ctx->make_actor<root_a>("root_a");
      }
    }

    void action_handle(const yaaf::envelope &) override {}

    yaaf::actor_action_when_error on_child_error(const actor_address &addr) {
      child_has_error = true;
      EXPECT_EQ(addr, root_addr);
      return yaaf::actor_action_when_error::RESUME;
    }

    yaaf::actor_address root_addr;
    bool child_has_error = false;
  };

  auto ctx = yaaf::context::make_context();
  auto core_addr = ctx->make_actor<core_a>("core_a");
  auto core_raw_ptr = ctx->actor_cast<core_a>(core_addr);

  while (core_raw_ptr->root_addr.empty()) {
    logger_info("wait while the core was not started...");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  auto root_address = core_raw_ptr->root_addr;

  auto root_ptr_raw = ctx->actor_cast<root_a>(root_address);
  while (!root_ptr_raw->is_on_start_called) {
    logger_info("wait while the root was not started...");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  auto root_weak_addr = ctx->get_address("/root/usr/core_a/root_a");
  EXPECT_FALSE(root_weak_addr.empty());
  auto root_weak = ctx->get_actor("/root/usr/core_a/root_a");
  auto sp = root_weak.lock();
  EXPECT_TRUE(sp != nullptr);

  EXPECT_EQ(root_ptr_raw->status().kind, yaaf::actor_status_kinds::NORMAL);

  std::unordered_set<yaaf::actor_address> children_addresses = root_ptr_raw->children;
  std::vector<yaaf::actor_ptr> children_actors;

  std::transform(
      children_addresses.cbegin(), children_addresses.cend(),
      std::back_inserter(children_actors),
      [ctx](const yaaf::actor_address addr) { return ctx->get_actor(addr).lock(); });

  for (auto &ac : children_actors) {
    EXPECT_EQ(ac->status().kind, yaaf::actor_status_kinds::NORMAL);
  }

  SECTION("context. root stoping") {
    ctx->stop_actor(root_address);

    for (auto &ac : children_actors) {
      EXPECT_TRUE(ac->status().kind == yaaf::actor_status_kinds::STOPED);
    }

    root_weak_addr = ctx->get_address("/root/usr/core_a/root_a");
    EXPECT_TRUE(root_weak_addr.empty());
    root_weak = ctx->get_actor("/root/usr/core_a/root_a");
    sp = root_weak.lock();
    EXPECT_FALSE(sp != nullptr);
    EXPECT_FALSE(core_raw_ptr->child_has_error);
  }

  SECTION("context. child stoping") {
    auto child = *children_addresses.begin();
    auto child_a = children_actors.front();
    ctx->stop_actor(child);

    EXPECT_TRUE(child_a->status().kind == yaaf::actor_status_kinds::STOPED);

    EXPECT_EQ(root_ptr_raw->stopped.size(), size_t(1));
    EXPECT_EQ(root_ptr_raw->stopped[child.get_id()], yaaf::actor_stopping_reason::MANUAL);
  }

  SECTION("context. child stoping with exception") {
    std::shared_ptr<root_a> raw_ptr = ctx->actor_cast<root_a>("/root/usr/core_a/root_a");

    SECTION("context. actor_action_when_error::STOP") {
      raw_ptr->on_error_flag = yaaf::actor_action_when_error::STOP;

      for (auto c : children_addresses) {
        ctx->send(c, std::string("bad cast"));
      }

      while (root_ptr_raw->stopped_childs_count != children_addresses.size()) {
        logger_info("wait while all childs is not stopped...");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      for (auto &ac : children_actors) {
        EXPECT_TRUE(ac->status().kind == yaaf::actor_status_kinds::STOPED);
      }

      EXPECT_EQ(root_ptr_raw->stopped.size(), size_t(3));
      for (auto &kv : root_ptr_raw->stopped) {
        EXPECT_EQ(kv.second, yaaf::actor_stopping_reason::EXCEPT);
      }

      EXPECT_FALSE(core_raw_ptr->child_has_error);
    }

    SECTION("context. actor_action_when_error::REINIT") {
      for (auto &c : children_actors) {
        dynamic_cast<child1_a *>(c.get())->is_on_start_called = false;
      }

      raw_ptr->on_error_flag = yaaf::actor_action_when_error::REINIT;

      for (auto c : children_addresses) {
        ctx->send(c, std::string("bad cast"));
      }

      bool is_end = false;
      while (!is_end) {
        is_end = std::all_of(
            children_actors.begin(), children_actors.end(),
            [](const yaaf::actor_ptr &aptr) {
              return dynamic_cast<const child1_a *>(aptr.get())->is_on_start_called;
            });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        logger_info("wait while all childs is not restarted...");
      }

      EXPECT_FALSE(core_raw_ptr->child_has_error);
    }

    SECTION("context. actor_action_when_error::RESUME") {
      raw_ptr->on_error_flag = yaaf::actor_action_when_error::RESUME;
      for (auto c : children_addresses) {
        ctx->send(c, std::string("bad cast"));
      }

      while (root_ptr_raw->statuses_count != children_addresses.size()) {
        logger_info("wait while all childs is not called...");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      for (auto &ac : children_actors) {
        EXPECT_TRUE(ac->status().kind == yaaf::actor_status_kinds::WITH_ERROR);
      }

      EXPECT_FALSE(core_raw_ptr->child_has_error);
    }

    SECTION("context. actor_action_when_error::ESCALATE") {
      raw_ptr->on_error_flag = yaaf::actor_action_when_error::ESCALATE;
      for (auto c : children_addresses) {
        ctx->send(c, std::string("bad cast"));
      }

      while (!core_raw_ptr->child_has_error) {
        logger_info("wait while core::on_child_error was not called ...");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
  }

  SECTION("context. children send calculation status after each apply") {
    for (auto c : children_addresses) {
      ctx->send(c, int(1));
    }

    while (root_ptr_raw->statuses_count != children_addresses.size()) {
      logger_info("wait while status from each child");
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    for (auto &kv : root_ptr_raw->statuses) {
      EXPECT_EQ(kv.second, yaaf::actor_status_kinds::NORMAL);
    }
  }

  children_actors.clear();
}

TEST_CASE("context. ping-pong", "[context]") {
  using namespace yaaf::utils;

  class pong_actor : public base_actor {
  public:
    void action_handle(const envelope &e) override {
      ENSURE(e.payload.is<int>());
      pongs++;
      auto ctx = get_context();
      if (ctx != nullptr) {
        ctx->send(e.sender, int(2));
      }
    }

    std::atomic_size_t pongs = 0;
  };

  class ping_actor : public base_actor {
    size_t _pongs_count;

  public:
    ping_actor(size_t pongs_count) : _pongs_count(pongs_count) {}

    void on_start() override {
      auto ctx = get_context();
      if (ctx != nullptr) {
        for (size_t i = 0; i < _pongs_count; ++i) {
          auto pong_addr = ctx->make_actor<pong_actor>("pong_" + std::to_string(i));
          ping(pong_addr);
        }
      }
    }

    void action_handle(const envelope &e) override {
      ENSURE(e.payload.is<int>());
      pings++;
      ping(e.sender);
    }

    void ping(const yaaf::actor_address &pong_addr) {
      auto ctx = get_context();
      if (ctx != nullptr) {
        ctx->send(pong_addr, int(1));
      }
    }
    std::atomic_size_t pings = 0;
  };

  yaaf::context::params_t ctx_params = yaaf::context::params_t::defparams();
  size_t pingers_count = 1;

  SECTION("context. ping-pong with default settings") {
    ctx_params = yaaf::context::params_t::defparams();

    SECTION("context: ping-pong 1") { pingers_count = 1; }
    SECTION("context: ping-pong 2") { pingers_count = 2; }
    SECTION("context: ping-pong 5") { pingers_count = 5; }
  }

  SECTION("context. ping-pong with custom settings") {
    ctx_params = yaaf::context::params_t::defparams();
    ctx_params.user_threads = 10;
    ctx_params.sys_threads = 2;

    SECTION("context: ping-pong 1") { pingers_count = 1; }
    SECTION("context: ping-pong 5") { pingers_count = 5; }
    SECTION("context: ping-pong 10") { pingers_count = 10; }
  }

  auto ctx = yaaf::context::make_context(ctx_params);

  std::vector<yaaf::actor_address> pingers(pingers_count);
  for (size_t i = 0; i < pingers_count; ++i) {
    pingers[i] = ctx->make_actor<ping_actor>("ping", pingers_count);
  }

  auto addr_to_pointer = [ctx](const yaaf::actor_address &addr) {
    auto ping_ptr = ctx->actor_cast<ping_actor>(addr);
    EXPECT_NE(ping_ptr, nullptr);
    return ping_ptr;
  };

  std::vector<std::shared_ptr<ping_actor>> pingers_raw_ptrs;
  pingers_raw_ptrs.reserve(pingers.size());
  std::transform(pingers.cbegin(), pingers.cend(), std::back_inserter(pingers_raw_ptrs),
                 addr_to_pointer);

  std::vector<size_t> pings_count;
  pings_count.reserve(pingers_count);
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::vector<size_t> pings_count_cur;
    pings_count_cur.reserve(pingers_count);

    std::transform(pingers_raw_ptrs.cbegin(), pingers_raw_ptrs.cend(),
                   std::back_inserter(pings_count_cur),
                   [](auto a) { return a->pings.load(); });

    if (!pings_count.empty()) {
      int eq_cnt = 0;
      for (size_t i = 0; i < pings_count.size(); ++i) {
        if (pings_count.at(i) == pings_count_cur.at(i)) {
          eq_cnt++;
        }
      }
      EXPECT_NE(eq_cnt, pings_count.size());
    }
    pings_count = std::move(pings_count_cur);
    auto all_more_than_100 = std::all_of(pings_count.cbegin(), pings_count.cend(),
                                         [](size_t p) { return p >= 100; });
    if (all_more_than_100) {
      break;
    } else {
      std::stringstream ss;
      ss << "[";
      for (auto &v : pings_count) {
        ss << v << " ";
      }
      ss << "]";
      logger_info("pings count < 100: ", ss.str());
    }
  }

  ctx->stop();
  ctx = nullptr;
}

namespace {
const std::string PP_ENAME = "ping pong exchange";
}

TEST_CASE("context. ping-pong over exchange", "[context]") {

  class pong_actor : public base_actor {
  public:
    void on_start() override {
      auto ctx = get_context();
      if (ctx != nullptr) {
        ENSURE(ctx->exchange_exists(PP_ENAME));
        ctx->subscribe_to_exchange(PP_ENAME);
      }
      started = true;
    }

    void action_handle(const envelope &e) override {
      if (!e.payload.is<int>()) {
        EXPECT_FALSE(true);
      }
      pongs++;
      auto sender_name = e.sender.get_pathname();
      if (sender_name != "/root/usr/ping") {
        EXPECT_FALSE(true);
      }
    }

    std::atomic_size_t pongs = 0;
    bool started = false;
  };

  class ping_actor : public base_actor {
    size_t _pongs_count;

  public:
    ping_actor() {}

    void on_start() override {
      auto ctx = get_context();
      if (ctx != nullptr) {
        ctx->create_exchange(PP_ENAME);
      }
    }

    void action_handle(const envelope &e) override {
      auto v = e.payload.cast<int>();
      pings++;
      ping(v);
    }

    void ping(int v) {
      auto ctx = get_context();
      if (ctx != nullptr) {
        ctx->publish(PP_ENAME, v);
      }
    }
    std::atomic_size_t pings = 0;
  };

  yaaf::context::params_t ctx_params = yaaf::context::params_t::defparams();
  size_t pongers_count = 1;

  SECTION("context. exhange with default settings") {
    ctx_params = yaaf::context::params_t::defparams();

    SECTION("context: exhange 1") { pongers_count = 1; }
    SECTION("context: exhange 2") { pongers_count = 2; }
    SECTION("context: exhange 5") { pongers_count = 5; }
  }

  SECTION("context. exhange with custom settings") {
    ctx_params = yaaf::context::params_t::defparams();
    ctx_params.user_threads = 10;
    ctx_params.sys_threads = 1;

    SECTION("context: exhange 1") { pongers_count = 1; }
    SECTION("context: exhange 5") { pongers_count = 5; }
    SECTION("context: exhange 10") { pongers_count = 10; }
  }

  auto ctx = yaaf::context::make_context(ctx_params);

  yaaf::actor_address pinger_addr = ctx->make_actor<ping_actor>("ping");
  std::vector<yaaf::actor_address> pongers(pongers_count);
  for (size_t i = 0; i < pongers_count; ++i) {
    pongers[i] = ctx->make_actor<pong_actor>("pong_" + std::to_string(i));
  }

  auto addr_to_pointer = [ctx](const yaaf::actor_address &addr) {
    auto raw_ptr = ctx->actor_cast<pong_actor>(addr);
    EXPECT_NE(raw_ptr, nullptr);
    return raw_ptr;
  };

  std::vector<std::shared_ptr<pong_actor>> pongers_raw_ptrs;
  pongers_raw_ptrs.reserve(pongers.size());
  std::transform(pongers.cbegin(), pongers.cend(), std::back_inserter(pongers_raw_ptrs),
                 addr_to_pointer);
  auto f_is_started = [](const std::shared_ptr<pong_actor> &v) { return v->started; };
  while (true) {
    auto is_started =
        std::all_of(pongers_raw_ptrs.begin(), pongers_raw_ptrs.end(), f_is_started);
    if (is_started) {
      break;
    }
    logger_info("wait pingers");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  std::vector<size_t> pongs_count;
  pongs_count.reserve(pongers_count);

  for (int i = 0; i < 100; ++i) {
    ctx->send(pinger_addr, int(i));
  }

  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::vector<size_t> pongs_count_cur;
    pongs_count_cur.reserve(pongers_count);

    std::transform(pongers_raw_ptrs.cbegin(), pongers_raw_ptrs.cend(),
                   std::back_inserter(pongs_count_cur),
                   [](auto a) { return a->pongs.load(); });

    if (!pongs_count.empty()) {
      int eq_cnt = 0;
      for (size_t i = 0; i < pongs_count.size(); ++i) {
        if (pongs_count.at(i) == pongs_count_cur.at(i)) {
          eq_cnt++;
        }
      }
      EXPECT_NE(eq_cnt, pongs_count.size());
    }
    pongs_count = std::move(pongs_count_cur);
    auto all_more_than_100 = std::all_of(pongs_count.cbegin(), pongs_count.cend(),
                                         [](size_t p) { return p >= 100; });
    if (all_more_than_100) {
      break;
    } else {
      std::stringstream ss;
      ss << "[";
      for (auto &v : pongs_count) {
        ss << v << " ";
      }
      ss << "]";
      std::cout << "pings count < 100: " << ss.str();
      logger_info("pings count < 100: ", ss.str());
    }
  }
  ctx->stop_actor(pinger_addr);
  for (auto &p : pongers) {
    ctx->stop_actor(p);
  }
  while (ctx->exchange_exists(PP_ENAME)) {
    logger_info("wait exchange cleanup");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  ctx->stop();
  ctx = nullptr;
}