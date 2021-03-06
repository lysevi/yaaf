#include <libyaaf/actor.h>
#include <libyaaf/mailbox.h>
#include <benchmark/benchmark.h>

using namespace yaaf;

static void BM_ActorCtor(benchmark::State &state) {
  auto clbk = [](envelope) {};
  for (auto _ : state) {
    auto ac = std::make_shared<actor_for_delegate>(actor_for_delegate::delegate_t(clbk));
    benchmark::DoNotOptimize(ac);
  }
}
BENCHMARK(BM_ActorCtor);
