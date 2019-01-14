#pragma once
#include <libnmq/utils/logger.h>
namespace microbenchmark_common {
class BenchmarkLogger : public nmq::utils::ILogger {
public:
  BenchmarkLogger() {}
  ~BenchmarkLogger() {}
  void message(nmq::utils::LOG_MESSAGE_KIND, const std::string &) {}
};

void replace_std_logger();
} // namespace microbenchmark_common