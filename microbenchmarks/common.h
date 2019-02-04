#pragma once
#include <libnmq/utils/logger.h>
namespace microbenchmark_common {
class BenchmarkLogger : public nmq::utils::logging::abstract_logger {
public:
  BenchmarkLogger() {}
  ~BenchmarkLogger() {}
  void message(nmq::utils::logging::message_kind, const std::string &) noexcept {}
};

void replace_std_logger();
} // namespace microbenchmark_common