#pragma once
#include <libyaaf/utils/logger.h>
namespace microbenchmark_common {
class BenchmarkLogger : public yaaf::utils::logging::abstract_logger {
public:
  BenchmarkLogger() {}
  ~BenchmarkLogger() {}
  void message(yaaf::utils::logging::message_kind, const std::string &) noexcept {}
};

void replace_std_logger();
} // namespace microbenchmark_common