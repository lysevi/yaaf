#include <libnmq/utils/logger.h>
#include <benchmark/benchmark.h>

// BENCHMARK_MAIN();
int main(int argc, char **argv) {
  auto _raw_ptr = new nmq::utils::logging::quiet_logger();
  auto _logger = nmq::utils::logging::abstract_logger_ptr{_raw_ptr};
  nmq::utils::logging::logger_manager::start(_logger);

  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv))
    return 1;
  ::benchmark::RunSpecifiedBenchmarks();
}