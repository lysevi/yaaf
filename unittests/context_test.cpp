#include "helpers.h"
#include <libyaaf/context.h>
#include <libyaaf/utils/logger.h>
#include <algorithm>

#include <catch.hpp>

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

  EXPECT_NE(c1_addr.to_string(), "null");
  EXPECT_NE(c2_addr.to_string(), "null");
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
  yaaf::actor_ptr aptr = ctx->get_actor(aptr_addr).lock();

  auto testable_a_ptr = dynamic_cast<testable_actor *>(aptr.get());

  EXPECT_TRUE(testable_a_ptr->status().kind == yaaf::actor_status_kinds::NORMAL);

  ctx->stop_actor(aptr_addr);

  SECTION("context. check start|stop flags") {
    EXPECT_TRUE(testable_a_ptr->is_on_init_called);
    EXPECT_TRUE(testable_a_ptr->is_on_start_called);
    EXPECT_TRUE(testable_a_ptr->is_on_stop_called);
    EXPECT_TRUE(testable_a_ptr->status().kind == yaaf::actor_status_kinds::STOPED);
  }
  ctx = nullptr;
}

TEST_CASE("context. hierarchy initialize", "[context]") {

  class child1_a : public yaaf::base_actor {
  public:
    child1_a() {}

    yaaf::actor_settings on_init(const yaaf::actor_settings &bs) override {
      EXPECT_TRUE(bs.stop_on_any_error);
      is_on_init_called = true;
      return yaaf::base_actor::on_init(bs);
    }
    void on_stop() override {
      is_on_stop_called = true;
      yaaf::base_actor::on_stop();
    }

    void action_handle(const yaaf::envelope &e) override { e.payload.cast<int>(); }

    bool is_on_init_called = false;
    bool is_on_stop_called = false;
  };

  class root_a : public yaaf::base_actor {
  public:
    root_a() {}

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
      EXPECT_FALSE(bs.stop_on_any_error);
      yaaf::actor_settings result = bs;
      result.stop_on_any_error = true;
      return yaaf::base_actor::on_init(result);
    }

    void on_start() override {
      auto ctx = get_context();

      if (ctx == nullptr) {
        throw std::logic_error("context is nullptr");
      }

      for (int i = 0; i < 3; ++i) {
        auto a = ctx->make_actor<child1_a>("child_" + std::to_string(i));
        children.push_back(a);
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
    std::vector<yaaf::actor_address> children;

    size_t stopped_childs_count = 0;
    std::map<yaaf::id_t, yaaf::actor_stopping_reason> stopped;

    size_t statuses_count = 0;
    std::map<yaaf::id_t, yaaf::actor_status_kinds> statuses;
  };

  auto ctx = yaaf::context::make_context();
  auto root_address = ctx->make_actor<root_a>("root_a");

  auto root_ptr = ctx->get_actor(root_address).lock();
  auto root_ptr_raw = dynamic_cast<root_a *>(root_ptr.get());
  while (!root_ptr_raw->is_on_start_called) {
    logger_info("wait while the root was not started...");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  auto root_weak = ctx->get_actor("/root/usr/root_a");
  auto sp = root_weak.lock();
  EXPECT_TRUE(sp != nullptr);

  EXPECT_EQ(root_ptr_raw->status().kind, yaaf::actor_status_kinds::NORMAL);

  std::vector<yaaf::actor_address> children_addresses = root_ptr_raw->children;
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

    root_weak = ctx->get_actor("/root/usr/root_a");
    sp = root_weak.lock();
    EXPECT_FALSE(sp != nullptr);
  }

  SECTION("context. child stoping") {
    auto child = children_addresses.front();
    auto child_a = children_actors.front();
    ctx->stop_actor(child);

    EXPECT_TRUE(child_a->status().kind == yaaf::actor_status_kinds::STOPED);

    EXPECT_EQ(root_ptr_raw->stopped.size(), size_t(1));
    EXPECT_EQ(root_ptr_raw->stopped[child.get_id()], yaaf::actor_stopping_reason::MANUAL);
  }

  SECTION("context. child stoping with exception") {
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

  root_ptr = nullptr;
  children_actors.clear();
}

TEST_CASE("context. ping-pong", "[context]") {

  class pong_actor : public base_actor {
  public:
    void action_handle(const envelope &e) override {
      auto v = e.payload.cast<int>();
      UNUSED(v);
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
      auto v = e.payload.cast<int>();
      UNUSED(v);
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
    auto ping_ptr = ctx->get_actor(addr);
    ping_actor *raw_ptr = nullptr;
    if (auto p = ping_ptr.lock()) {
      raw_ptr = dynamic_cast<ping_actor *>(p.get());
    } else {
      EXPECT_FALSE(true);
    }
    return raw_ptr;
  };

  std::vector<ping_actor *> pingers_raw_ptrs;
  pingers_raw_ptrs.reserve(pingers.size());
  std::transform(pingers.cbegin(), pingers.cend(), std::back_inserter(pingers_raw_ptrs),
                 addr_to_pointer);

  while (true) {
    std::vector<size_t> pings_count;
    pings_count.reserve(pingers_count);

    std::transform(pingers_raw_ptrs.cbegin(), pingers_raw_ptrs.cend(),
                   std::back_inserter(pings_count),
                   [](const ping_actor *a) { return a->pings.load(); });

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
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  ctx->stop();
  ctx = nullptr;
}