#include <libnmq/context.h>
#include <benchmark/benchmark.h>

using namespace nmq;

static void BM_Context(benchmark::State &state) {
  for (auto _ : state) {
  }
}
BENCHMARK(BM_Context);
