#include <libnmq/context.h>
#include <benchmark/benchmark.h>

using namespace nmq;

static void BM_Context(benchmark::State &state) {
  auto ctx = std::make_shared<context>(context::params_t::defparams());

  auto c1 = [](nmq::envelope e) {
    auto v = boost::any_cast<int>(e.payload);
	ENSURE(v==1);
  };

  auto c1_addr = ctx->add_actor(actor_for_delegate::delegate_t(c1));

  for (auto _ : state) {
    c1_addr.send(c1_addr, int(1));
  }
  ctx=nullptr;
}
BENCHMARK(BM_Context);
