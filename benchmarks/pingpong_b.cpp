#include <libnmq/context.h>
#include <libnmq/utils/logger.h>
#include <iostream>

#include <cxxopts.hpp>

using namespace nmq;

std::atomic_size_t pings = 0;
std::atomic_size_t pongs = 0;

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
};

class ping_actor : public base_actor {
public:
  ping_actor(size_t pongs_count_) { pongs_count = pongs_count_; }
  void on_start() override {
    auto ctx = get_context();
    if (ctx != nullptr) {
      for (size_t i = 0; i < pongs_count; ++i) {
        auto paddr = ctx->make_actor<pong_actor>("pong_" + std::to_string(i));
        pongs_addrs.push_back(paddr);
        ping(paddr);
      }
    }
  }

  void action_handle(const envelope &e) override {
    auto v = boost::any_cast<int>(e.payload);
    UNUSED(v);
    pings++;
    ping(e.sender);
  }

  void ping(const nmq::actor_address &pa) {
    auto ctx = get_context();
    if (ctx != nullptr) {
      ctx->send(pa, int(1));
    }
  }
  size_t pongs_count;
  std::vector<nmq::actor_address> pongs_addrs;
};

int steps = 10;
size_t pongs_count = 1;
size_t userspace_threads = 1;
nmq::utils::logging::abstract_logger *_raw_logger_ptr = nullptr;

void parse_args(int argc, char **argv) {
  cxxopts::Options options("ping-pong", "benchmark via ping-pong");
  options.allow_unrecognised_options();
  options.positional_help("[optional args]").show_positional_help();

  auto add_o = options.add_options();
  add_o("v,verbose", "Enable debugging");
  add_o("h,help", "Help");
  add_o("s,steps", "Steps count", cxxopts::value<int>(steps));
  add_o("p,pongers", "Pongers count", cxxopts::value<size_t>(pongs_count));
  add_o("u,userspace_threads", "Userspace threads",
        cxxopts::value<size_t>(userspace_threads));

  try {
    cxxopts::ParseResult result = options.parse(argc, argv);

    if (result["help"].as<bool>()) {
      std::cout << options.help() << std::endl;
      std::exit(0);
    }

    if (result["verbose"].as<bool>()) {
      _raw_logger_ptr = new nmq::utils::logging::console_logger();
    } else {
      _raw_logger_ptr = new nmq::utils::logging::quiet_logger();
    }
  } catch (cxxopts::OptionException &ex) {
    std::cerr << ex.what() << std::endl;
  }

  std::cout << "pingers: " << pongs_count << std::endl;
  std::cout << "steps: " << steps << std::endl;
  std::cout << "userspace threads: " << userspace_threads << std::endl;
}

int main(int argc, char **argv) {
  parse_args(argc, argv);

  auto _logger = nmq::utils::logging::abstract_logger_ptr{_raw_logger_ptr};
  nmq::utils::logging::logger_manager::start(_logger);

  context::params_t params = context::params_t::defparams();
  params.user_threads = userspace_threads;
  params.sys_threads = 1;

  auto ctx = nmq::context::make_context(params);

  ctx->make_actor<ping_actor>("ping", pongs_count);

  std::this_thread::sleep_for(std::chrono::seconds(1));

  for (int i = 0; i < steps; ++i) {
    size_t last_ping = pings.load();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    size_t new_ping = pings.load();
    size_t diff = (new_ping - last_ping)/pongs_count;
    std::cout << "#: " << i << " ping-pong speed: " << diff << " per.sec." << std::endl;

    ENSURE(diff != size_t(0));
  }
}