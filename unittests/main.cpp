#define CATCH_CONFIG_RUNNER
#include <libnmq/utils/logger.h>
#include <catch.hpp>
#include <cstring>
#include <iostream>
#include <list>
#include <sstream>

class UnitTestLogger : public nmq::utils::logging::abstract_logger {
public:
  static bool verbose;
  UnitTestLogger() {}
  ~UnitTestLogger() {}

  void message(nmq::utils::logging::message_kind kind, const std::string &msg) noexcept {
    std::stringstream ss;
    switch (kind) {
    case nmq::utils::logging::message_kind::fatal:
      ss << "[err] " << msg << std::endl;
      break;
    case nmq::utils::logging::message_kind::info:
      ss << "[inf] " << msg << std::endl;
      break;
    case nmq::utils::logging::message_kind::warn:
      ss << "[wrn] " << msg << std::endl;
      break;
    case nmq::utils::logging::message_kind::message:
      ss << "[dbg] " << msg << std::endl;
      break;
    }

    if (kind == nmq::utils::logging::message_kind::fatal) {
      std::cerr << ss.str();
    } else {
      if (verbose) {
        std::cout << ss.str();
      } else {
        _messages.push_back(ss.str());
      }
    }
  }

  void dump_all() {
    for (auto m : _messages) {
      std::cerr << m;
    }
  }

private:
  std::list<std::string> _messages;
};

bool UnitTestLogger::verbose = false;

struct LoggerControl : Catch::TestEventListenerBase {
  using TestEventListenerBase::TestEventListenerBase; // inherit constructor

  virtual void testCaseStarting(Catch::TestCaseInfo const &) override {
    _raw_ptr = new UnitTestLogger();
    _logger = nmq::utils::logging::abstract_logger_ptr{_raw_ptr};
    nmq::utils::logging::logger_manager::start(_logger);
  }

  virtual void testCaseEnded(Catch::TestCaseStats const &testCaseStats) override {
    if (testCaseStats.testInfo.expectedToFail()) {
      _raw_ptr->dump_all();
    }
    nmq::utils::logging::logger_manager::stop();
    _logger = nullptr;
  }
  UnitTestLogger *_raw_ptr;
  nmq::utils::logging::abstract_logger_ptr _logger;
};

CATCH_REGISTER_LISTENER(LoggerControl);

int main(int argc, char **argv) {
  int _argc = argc;
  char **_argv = argv;
  for (int i = 0; i < argc; ++i) {
    if (std::strcmp(argv[i], "--verbose") == 0) {
      UnitTestLogger::verbose = true;
      _argc--;
      _argv = new char *[_argc];
      int r_pos = 0, w_pos = 0;
      for (int a = 0; a < argc; ++a) {
        if (a != i) {

          _argv[w_pos] = argv[r_pos];
          w_pos++;
        }
        r_pos++;
      }
      break;
      ;
    }
  }

  Catch::Session sesssion;
  sesssion.configData().showDurations = Catch::ShowDurations::OrNot::Always;
  int result = sesssion.run(_argc, _argv);
  if (UnitTestLogger::verbose) {
    delete[] _argv;
  }
  return (result < 0xff ? result : 0xff);
}
