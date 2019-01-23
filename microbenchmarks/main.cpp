#include <libnmq/utils/logger.h>
#include <benchmark/benchmark.h>

// BENCHMARK_MAIN();
int main(int argc, char **argv) {
  auto _raw_ptr = new nmq::utils::QuietLogger();
  auto _logger = nmq::utils::ILogger_ptr{_raw_ptr};
  nmq::utils::LogManager::start(_logger);

  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv))
    return 1;
  ::benchmark::RunSpecifiedBenchmarks();
}