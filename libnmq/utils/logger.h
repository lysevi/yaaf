#pragma once

#include <libnmq/exports.h>
#include <libnmq/utils/async/locker.h>
#include <libnmq/utils/strings.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace nmq {
namespace utils {

enum class LOG_MESSAGE_KIND { MESSAGE, INFO, FATAL };

class ILogger {
public:
  virtual void message(LOG_MESSAGE_KIND kind, const std::string &msg) noexcept = 0;
  virtual ~ILogger() {}
};

using ILogger_ptr = std::shared_ptr<ILogger>;

class ConsoleLogger : public ILogger {
public:
  EXPORT void message(LOG_MESSAGE_KIND kind, const std::string &msg) noexcept override;
};

class QuietLogger : public ILogger {
public:
  EXPORT void message(LOG_MESSAGE_KIND kind, const std::string &msg) noexcept override;
};

enum class Verbose { Verbose, Debug, Quiet };

class LogManager {
public:
  EXPORT static Verbose verbose;
  LogManager(ILogger_ptr &logger);

  EXPORT static void start(ILogger_ptr &logger);
  EXPORT static void stop();
  EXPORT static LogManager *instance() noexcept;

  EXPORT void message(LOG_MESSAGE_KIND kind, const std::string &msg) noexcept;

  template <typename... T>
  void variadic_message(LOG_MESSAGE_KIND kind, T &&... args) noexcept {
    auto str_message = utils::strings::args_to_string(args...);
    this->message(kind, str_message);
  }

private:
  static std::shared_ptr<LogManager> _instance;
  static utils::async::locker _locker;
  utils::async::locker _msg_locker;
  ILogger_ptr _logger;
};
} // namespace utils

template <typename... T> void logger(T &&... args) noexcept {
  nmq::utils::LogManager::instance()->variadic_message(
      nmq::utils::LOG_MESSAGE_KIND::MESSAGE, args...);
}

template <typename... T> void logger_info(T &&... args) noexcept {
  nmq::utils::LogManager::instance()->variadic_message(nmq::utils::LOG_MESSAGE_KIND::INFO,
                                                       args...);
}

template <typename... T> void logger_fatal(T &&... args) noexcept {
  nmq::utils::LogManager::instance()->variadic_message(
      nmq::utils::LOG_MESSAGE_KIND::FATAL, args...);
}
} // namespace nmq
