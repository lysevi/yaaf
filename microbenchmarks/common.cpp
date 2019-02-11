#include "common.h"

namespace microbenchmark_common {
void replace_std_logger() {
  auto _raw_ptr = new BenchmarkLogger();
  auto _logger = yaaf::utils::logging::abstract_logger_ptr{_raw_ptr};

  yaaf::utils::logging::logger_manager::start(_logger);
}
} // namespace microbenchmark_common