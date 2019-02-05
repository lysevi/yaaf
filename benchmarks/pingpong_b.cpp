#include <libnmq/context.h>
#include <libnmq/utils/logger.h>
#include <iostream>

using namespace nmq;

std::atomic_size_t pings = 0;
std::atomic_size_t pongs = 0;

std::shared_ptr<context> ctx;

class pong_actor : public base_actor {
public:
  void action_handle(envelope &e) override {
    auto v = boost::any_cast<int>(e.payload);
    UNUSED(v);
    pongs++;
    e.sender.send(*self_addr(), int(2));
  }
};

class ping_actor : public base_actor {
public:
  void on_start() override {
    pong_addr = self_addr()->ctx()->add_actor(std::make_shared<pong_actor>());
  }
  void action_handle(envelope &e) override {
    auto v = boost::any_cast<int>(e.payload);
    UNUSED(v);
    pings++;

    pong_addr.send(*self_addr(), int(1));
  }
  nmq::actor_address pong_addr;
};

int main(int argc, char **argv) {
  UNUSED(argc);
  UNUSED(argv);
  auto _raw_ptr = new nmq::utils::logging::quiet_logger();
  auto _logger = nmq::utils::logging::abstract_logger_ptr{_raw_ptr};
  nmq::utils::logging::logger_manager::start(_logger);

  context::params_t params = context::params_t::defparams();
  params.user_threads = 1;
  params.sys_threads = 1;
  ctx = std::make_shared<context>(params);
  auto ping_ptr = std::make_shared<ping_actor>();

  auto c1_addr = ctx->add_actor(ping_ptr);

  c1_addr.send(nmq::actor_address(), int(2));

  for (int i = 0;; ++i) {
    size_t last_ping = pings.load();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    size_t new_ping = pings.load();

    std::cout << "#: " << i << " ping/pong speed: " << (new_ping - last_ping) / 1000.0
              << " per.sec." << std::endl;
  }
}