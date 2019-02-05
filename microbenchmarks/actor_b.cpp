#include <libnmq/actor.h>
#include <libnmq/mailbox.h>
#include <benchmark/benchmark.h>

using namespace nmq;

static void BM_ActorCtor(benchmark::State &state) {
  auto clbk = [](envelope) {};
  for (auto _ : state) {
    actor_ptr ac = std::make_shared<actor_for_delegate>(actor_for_delegate::delegate_t(clbk));
    benchmark::DoNotOptimize(ac);
  }
}
BENCHMARK(BM_ActorCtor);
