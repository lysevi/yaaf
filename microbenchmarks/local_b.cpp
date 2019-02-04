#include <libnmq/local/queue.h>
#include <benchmark/benchmark.h>

using namespace nmq;
using namespace nmq::local;

class Local : public benchmark::Fixture {
  virtual void SetUp(const ::benchmark::State &) {}

  virtual void TearDown(const ::benchmark::State &) {}

public:
};

BENCHMARK_DEFINE_F(Local, FixedQueuePush)(benchmark::State &state) {
  for (auto _ : state) {
    queue<int> fq(1024);
    for (int i = 0; i < 1024; ++i) {
      bool f = fq.try_push(i);
      if (!f) {
        break;
      }
    }
  }
}
BENCHMARK_REGISTER_F(Local, FixedQueuePush);
