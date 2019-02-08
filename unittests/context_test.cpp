#include "helpers.h"
#include <libnmq/context.h>
#include <libnmq/utils/logger.h>
#include <boost/range/algorithm.hpp>
#include <catch.hpp>

using namespace nmq;
using namespace nmq::utils::logging;

TEST_CASE("context. name", "[context]") {
  auto ctx = nmq::context::make_context();
  auto ctx2 = nmq::context::make_context();

  EXPECT_NE(ctx->name(), ctx2->name());
  ctx = nullptr;
  ctx2 = nullptr;
}

TEST_CASE("context. sending", "[context]") {
  auto ctx = nmq::context::make_context();
  int summ = 0;
  auto c1 = [&summ](nmq::envelope e) {
    auto v = boost::any_cast<int>(e.payload);
    summ += v;
  };

  auto c2 = [](nmq::envelope) {};
  auto c1_addr = ctx->make_actor<nmq::actor_for_delegate>("c1", c1);
  auto c2_addr = ctx->make_actor<nmq::actor_for_delegate>("c2", c2);

  EXPECT_NE(c1_addr.to_string(), "null");
  EXPECT_NE(c2_addr.to_string(), "null");
  EXPECT_NE(c1_addr.get_id(), c2_addr.get_id());

  SECTION("context. many values") {
    nmq::envelope e;
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
    ctx->send(c1_addr, nmq::envelope{int(1), c2_addr});
  }

  ctx = nullptr;
}

TEST_CASE("context. actor_start_stop", "[context]") {
  class testable_actor : public nmq::base_actor {
  public:
    testable_actor(int ctor_arg_) : ctor_arg(ctor_arg_) {}

    nmq::actor_settings on_init(const nmq::actor_settings &bs) override {
      auto ctx = get_context();
      if (ctx == nullptr) {
        throw std::logic_error("context is nullptr");
      }
      auto n = ctx->name();
      EXPECT_TRUE(n != std::string(""));

      is_on_init_called = true;
      return nmq::base_actor::on_init(bs);
    }
    void on_start() override {
      is_on_start_called = true;
      nmq::base_actor::on_start();
    }
    void on_stop() override {
      is_on_stop_called = true;
      nmq::base_actor::on_stop();
    }

    void action_handle(const nmq::envelope &) override {}

    int ctor_arg;
    bool is_on_init_called = false;
    bool is_on_start_called = false;
    bool is_on_stop_called = false;
  };

  auto ctx = nmq::context::make_context();

  auto aptr_addr = ctx->make_actor<testable_actor>("testable", int(1));
  nmq::actor_ptr aptr = ctx->get_actor(aptr_addr).lock();

  auto testable_a_ptr = dynamic_cast<testable_actor *>(aptr.get());

  EXPECT_TRUE(testable_a_ptr->status().kind == nmq::actor_status_kinds::NORMAL);

  ctx->stop_actor(aptr_addr);

  SECTION("context. check start|stop flags") {
    EXPECT_TRUE(testable_a_ptr->is_on_init_called);
    EXPECT_TRUE(testable_a_ptr->is_on_start_called);
    EXPECT_TRUE(testable_a_ptr->is_on_stop_called);
    EXPECT_TRUE(testable_a_ptr->status().kind == nmq::actor_status_kinds::STOPED);
  }
  ctx = nullptr;
}

TEST_CASE("context. hierarchy initialize", "[context]") {
  using namespace boost;

  class child1_a : public nmq::base_actor {
  public:
    child1_a() {}

    nmq::actor_settings on_init(const nmq::actor_settings &bs) override {
      EXPECT_TRUE(bs.stop_on_any_error);
      is_on_init_called = true;
      return nmq::base_actor::on_init(bs);
    }
    void on_stop() override {
      is_on_stop_called = true;
      nmq::base_actor::on_stop();
    }

    void action_handle(const nmq::envelope &e) override {
      boost::any_cast<int>(e.payload);
    }

    bool is_on_init_called = false;
    bool is_on_stop_called = false;
  };

  class root_a : public nmq::base_actor {
  public:
    root_a() {}

    void on_child_stopped(const nmq::actor_address &addr,
                          nmq::actor_stopping_reason reason) override {
      stopped_childs_count++;
      stopped[addr.get_id()] = reason;
    }

    void on_child_status(const nmq::actor_address &addr,
                         nmq::actor_status_kinds k) override {
      statuses_count++;
      statuses[addr.get_id()] = k;
    }

    nmq::actor_settings on_init(const nmq::actor_settings &bs) override {
      EXPECT_FALSE(bs.stop_on_any_error);
      nmq::actor_settings result = bs;
      result.stop_on_any_error = true;
      return nmq::base_actor::on_init(result);
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
      nmq::base_actor::on_start();
    }

    void on_stop() override {
      is_on_stop_called = true;
      nmq::base_actor::on_stop();
    }

    void action_handle(const nmq::envelope &) override {}

    bool is_on_start_called = false;
    bool is_on_stop_called = false;
    std::vector<nmq::actor_address> children;

    size_t stopped_childs_count = 0;
    std::map<nmq::id_t, nmq::actor_stopping_reason> stopped;

    size_t statuses_count = 0;
    std::map<nmq::id_t, nmq::actor_status_kinds> statuses;
  };

  auto ctx = nmq::context::make_context();
  auto root_address = ctx->make_actor<root_a>("root_a");

  auto root_ptr = ctx->get_actor(root_address).lock();
  auto root_ptr_raw = dynamic_cast<root_a *>(root_ptr.get());
  while (!root_ptr_raw->is_on_start_called) {
    logger_info("wait while the root was not started...");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  EXPECT_TRUE(root_ptr_raw->status().kind == nmq::actor_status_kinds::NORMAL);

  std::vector<nmq::actor_address> children_addresses = root_ptr_raw->children;
  std::vector<nmq::actor_ptr> children_actors;

  range::transform(
      children_addresses, std::back_inserter(children_actors),
      [ctx](nmq::actor_address addr) { return ctx->get_actor(addr).lock(); });

  for (auto &ac : children_actors) {
    EXPECT_TRUE(ac->status().kind == nmq::actor_status_kinds::NORMAL);
  }

  SECTION("context. root stoping") {
    ctx->stop_actor(root_address);

    for (auto &ac : children_actors) {
      EXPECT_TRUE(ac->status().kind == nmq::actor_status_kinds::STOPED);
    }
  }

  SECTION("context. child stoping") {
    auto child = children_addresses.front();
    auto child_a = children_actors.front();
    ctx->stop_actor(child);

    EXPECT_TRUE(child_a->status().kind == nmq::actor_status_kinds::STOPED);

    EXPECT_EQ(root_ptr_raw->stopped.size(), size_t(1));
    EXPECT_EQ(root_ptr_raw->stopped[child.get_id()], nmq::actor_stopping_reason::MANUAL);
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
      EXPECT_TRUE(ac->status().kind == nmq::actor_status_kinds::STOPED);
    }

    EXPECT_EQ(root_ptr_raw->stopped.size(), size_t(3));
    for (auto &kv : root_ptr_raw->stopped) {
      EXPECT_EQ(kv.second, nmq::actor_stopping_reason::EXCEPT);
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
      EXPECT_EQ(kv.second, nmq::actor_status_kinds::NORMAL);
    }
  }

  root_ptr = nullptr;
  children_actors.clear();
}

TEST_CASE("context. ping-pong", "[context]") {

  class pong_actor : public base_actor {
  public:
    void action_handle(const envelope &e) override {
      auto v = boost::any_cast<int>(e.payload);
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
  public:
    void on_start() override {
      auto ctx = get_context();
      if (ctx != nullptr) {
        pong_addr = ctx->make_actor<pong_actor>("pong");
      }
      ping();
    }

    void action_handle(const envelope &e) override {
      auto v = boost::any_cast<int>(e.payload);
      UNUSED(v);
      pings++;
      ping();
    }

    void ping() {
      auto ctx = get_context();
      if (ctx != nullptr) {
        ctx->send(pong_addr, int(1));
      }
    }
    std::atomic_size_t pings = 0;
    nmq::actor_address pong_addr;
  };

  nmq::context::params_t ctx_params;
  SECTION("context. ping-pong with default settings") {
    ctx_params = nmq::context::params_t::defparams();
  }

  SECTION("context. ping-pong with custom settings") {
    ctx_params = nmq::context::params_t::defparams();
    ctx_params.user_threads = 2;
    ctx_params.sys_threads = 2;
  }

  auto ctx = nmq::context::make_context(ctx_params);
  auto ping_addr = ctx->make_actor<ping_actor>("ping");

  auto ping_ptr = ctx->get_actor(ping_addr);
  ping_actor *ping_raw_ptr = nullptr;
  if (auto p = ping_ptr.lock()) {
    ping_raw_ptr = dynamic_cast<ping_actor *>(p.get());
  } else {
    EXPECT_FALSE(true);
  }

  while (ping_raw_ptr->pings.load() < 100) {
    logger_info("ping_act_ptr->pings.load() < 100");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  ctx->stop();
  ctx = nullptr;
}