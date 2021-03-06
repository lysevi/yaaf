#include <libyaaf/context.h>
#include <benchmark/benchmark.h>

using namespace yaaf;

static void BM_Context(benchmark::State &state) {
  auto ctx = std::make_shared<context>(context::params_t::defparams());

  auto c1 = [](yaaf::envelope e) {
    auto v = e.payload.cast<int>();
    UNUSED(v);
    ENSURE(v == 1);
  };

  auto c1_addr = ctx->make_actor<actor_for_delegate>("c1", c1);

  for (auto _ : state) {
    ctx->send(c1_addr, int(1));
  }
  ctx = nullptr;
}
BENCHMARK(BM_Context);
