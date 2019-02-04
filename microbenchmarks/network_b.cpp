#include <libnmq/network/message.h>
#include <benchmark/benchmark.h>

using namespace nmq;
using namespace nmq::network;

class Network : public benchmark::Fixture {
  virtual void SetUp(const ::benchmark::State &) {}

  virtual void TearDown(const ::benchmark::State &) {}

public:
};

BENCHMARK_DEFINE_F(Network, MessageAlloc)(benchmark::State &state) {
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(std::make_unique<message>(512));
  }
}
BENCHMARK_REGISTER_F(Network, MessageAlloc);
