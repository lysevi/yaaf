#include <libnmq/lockfree/queue.h>
#include <benchmark/benchmark.h>

using namespace nmq;
using namespace nmq::lockfree;

class Lockfree : public benchmark::Fixture {
  virtual void SetUp(const ::benchmark::State &) {}

  virtual void TearDown(const ::benchmark::State &) {}

public:
};

BENCHMARK_DEFINE_F(Lockfree, FixedQueuePush)(benchmark::State &state) {
  for (auto _ : state) {
    Queue<int> fq(1024);
    for (int i = 0; i < 1024; ++i) {
      bool f = fq.tryPush(i);
      if (!f) {
        break;
      }
    }
  }
}
BENCHMARK_REGISTER_F(Lockfree, FixedQueuePush);
