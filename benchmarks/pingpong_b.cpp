#include <libnmq/context.h>
#include <libnmq/utils/logger.h>
#include <iostream>

using namespace nmq;

actor_address *c1_addr_ptr;
actor_address *c2_addr_ptr;

class ping_actor : public base_actor {
public:
  void action_handle(envelope & /*e*/) override {}
};

int main(int argc, char **argv) {
  UNUSED(argc);
  UNUSED(argv);
  auto _raw_ptr = new nmq::utils::logging::quiet_logger();
  auto _logger = nmq::utils::logging::abstract_logger_ptr{_raw_ptr};
  nmq::utils::logging::logger_manager::start(_logger);

  context::params_t params = context::params_t::defparams();
  params.user_threads = 1;
  auto ctx = std::make_shared<context>(params);

  std::atomic_size_t pings = 0;
  std::atomic_size_t pongs = 0;

  auto c1 = [&](nmq::envelope e) {
    auto v = boost::any_cast<int>(e.payload);
    UNUSED(v);
    pings++;
    if (e.sender.empty()) {
      c2_addr_ptr->send(*c1_addr_ptr, int(1));
    } else {
      e.sender.send(*c1_addr_ptr, int(1));
    }
  };

  auto c2 = [&](nmq::envelope e) {
    auto v = boost::any_cast<int>(e.payload);
    UNUSED(v);
    pongs++;
    e.sender.send(*c2_addr_ptr, int(2));
  };

  auto c1_addr = ctx->add_actor(actor_for_delegate::delegate_t(c1));
  auto c2_addr = ctx->add_actor(actor_for_delegate::delegate_t(c2));
  c1_addr_ptr = &c1_addr;
  c2_addr_ptr = &c2_addr;

  c1_addr.send(c2_addr, int(2));

  for (int i = 0;; ++i) {
    size_t last_ping = pings.load();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    size_t new_ping = pings.load();

    std::cout << "#: " << i << " ping/pong speed: " << (new_ping - last_ping) / 1000.0
              << " per.sec." << std::endl;
  }
}