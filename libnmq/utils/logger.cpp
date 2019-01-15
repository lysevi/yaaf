#include <libnmq/utils/logger.h>
#include <iostream>

using namespace nmq::utils;
using namespace nmq::utils::async;

std::shared_ptr<LogManager> LogManager::_instance = nullptr;
nmq::utils::async::locker LogManager::_locker;

Verbose LogManager::verbose = Verbose::Debug;

void LogManager::start(ILogger_ptr &logger) {
  if (_instance == nullptr) {
    _instance = std::shared_ptr<LogManager>{new LogManager(logger)};
  }
}

void LogManager::stop() {
  _instance = nullptr;
}

LogManager *LogManager::instance() {
  auto tmp = _instance.get();
  if (tmp == nullptr) {
    std::lock_guard<locker> lock(_locker);
    tmp = _instance.get();
    if (tmp == nullptr) {
      ILogger_ptr l = std::make_shared<ConsoleLogger>();
      _instance = std::make_shared<LogManager>(l);
      tmp = _instance.get();
    }
  }
  return tmp;
}

LogManager::LogManager(ILogger_ptr &logger) {
  _logger = logger;
}

void LogManager::message(LOG_MESSAGE_KIND kind, const std::string &msg) {
  std::lock_guard<utils::async::locker> lg(_msg_locker);
  _logger->message(kind, msg);
}

void ConsoleLogger::message(LOG_MESSAGE_KIND kind, const std::string &msg) {
  if (LogManager::verbose == Verbose::Quiet) {
    return;
  }
  switch (kind) {
  case LOG_MESSAGE_KIND::FATAL:
    std::cerr << "[err] " << msg << std::endl;
    break;
  case LOG_MESSAGE_KIND::INFO:
    std::cout << "[inf] " << msg << std::endl;
    break;
  case LOG_MESSAGE_KIND::MESSAGE:
    if (LogManager::verbose == Verbose::Debug) {
      std::cout << "[dbg] " << msg << std::endl;
    }
    break;
  }
}
