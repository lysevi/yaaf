#include <libyaaf/utils/async/thread_manager.h>
#include <benchmark/benchmark.h>

using namespace yaaf;
using namespace yaaf::utils::async;

using namespace yaaf::utils::async;

const thread_kind_t tk = 1;
const size_t threads_count = 2;

class ThreadPool_b  : public benchmark::Fixture {
  virtual void SetUp(const ::benchmark::State &) {
    tr_pool = new threads_pool(threads_pool::params_t(threads_count, tk));
  }

  virtual void TearDown(const ::benchmark::State &) { delete tr_pool; }

public:
  threads_pool *tr_pool;
};

BENCHMARK_DEFINE_F(ThreadPool_b, repeated)(benchmark::State &state) {
  task at = [](const thread_info &) { return CONTINUATION_STRATEGY::SINGLE; };
  while (state.KeepRunning()) {
    auto tresult = tr_pool->post(wrap_task(at));
    tresult->wait();
  }
}
BENCHMARK_REGISTER_F(ThreadPool_b, repeated);
