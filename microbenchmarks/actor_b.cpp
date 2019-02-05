#include <libnmq/actor.h>
#include <libnmq/mailbox.h>
#include <benchmark/benchmark.h>

using namespace nmq;

static void BM_ActorCtor(benchmark::State &state) {
  auto clbk = [](nmq::actor_weak, envelope) {};
  for (auto _ : state) {
    nmq::actor_ptr actor =
        std::make_shared<nmq::actor>(nmq::actor::delegate_t(clbk));
    benchmark::DoNotOptimize(actor);
  }
}
BENCHMARK(BM_ActorCtor);
