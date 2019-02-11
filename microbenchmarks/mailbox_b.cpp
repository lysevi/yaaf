#include <libyaaf/context.h>
#include <benchmark/benchmark.h>

using namespace yaaf;

static void BM_MailBoxIsEmpty(benchmark::State &state) {
  mailbox mbox;
  for (auto _ : state) {
    mbox.empty();
  }
}
BENCHMARK(BM_MailBoxIsEmpty);

static void BM_MailBoxWrite(benchmark::State &state) {
  mailbox mbox;
  for (auto _ : state) {
    envelope ep;
    ep.payload = std::string("benchmark value");
    mbox.push(ep);
  }
}
BENCHMARK(BM_MailBoxWrite);


static void BM_MailBoxWriteMove(benchmark::State &state) {
  mailbox mbox;
  for (auto _ : state) {
    envelope ep;
    ep.payload = std::string("benchmark value");
    mbox.push(std::move(ep));
  }
}
BENCHMARK(BM_MailBoxWriteMove);
