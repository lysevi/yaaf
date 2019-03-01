#include <libyaaf/context.h>
#include <libyaaf/utils/logger.h>
#include <iostream>

std::atomic_size_t pings;

class pong_actor final : public yaaf::base_actor {
public:
  void action_handle(const yaaf::envelope &e) override {
    auto v = e.payload.cast<int>();
    std::cout << "pong: " << v << std::endl;
    auto ctx = get_context();
    if (ctx != nullptr) {
      ctx->send(e.sender, v + 1);
    }
  }
};

class ping_actor final : public yaaf::base_actor {
public:
  ping_actor() {}

  void on_start() override {
    auto ctx = get_context();
    if (ctx != nullptr) {
      pong_addr = ctx->make_actor<pong_actor>("pong");
      ping(pong_addr, 0);
    }
  }

  void action_handle(const yaaf::envelope &e) override {
    auto v = e.payload.cast<int>();
    std::cout << "ping: " << v << std::endl;
    pings += 1;
    ping(e.sender, v + 1);
  }

  void ping(const yaaf::actor_address &pa, int v) {
    auto ctx = get_context();
    if (ctx != nullptr) {
      ctx->send(pa, v);
    }
  }
  yaaf::actor_address pong_addr;
};

int main(int, char **) {

  auto _raw_logger_ptr = new yaaf::utils::logging::quiet_logger();
  auto _logger = yaaf::utils::logging::abstract_logger_ptr{_raw_logger_ptr};
  yaaf::utils::logging::logger_manager::start(_logger);

  yaaf::context::params_t params = yaaf::context::params_t::defparams();
  auto ctx = yaaf::context::make_context(params);

  ctx->make_actor<ping_actor>("ping");

  std::this_thread::sleep_for(std::chrono::seconds(1));

  while (true) {
    size_t last_ping = pings.load();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    size_t new_ping = pings.load();
    size_t diff = (new_ping - last_ping);
    std::cout << " ping-pong speed: " << diff << " per.sec." << std::endl;
  }
}