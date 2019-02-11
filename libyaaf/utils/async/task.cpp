#include <libyaaf/utils/async/task.h>
#include <libyaaf/utils/logger.h>

using namespace yaaf::utils;
using namespace yaaf::utils::logging;
using namespace yaaf::utils::async;

task_wrapper::task_wrapper(task &t, const std::string &_function, const std::string &file,
                           int line): _tinfo() {
  priority = TASK_PRIORITY::DEFAULT;
  _task = t;
  _parent_function = _function;
  _code_file = file;
  _code_line = line;
  _result = std::make_shared<task_result>();
}

task_wrapper::task_wrapper(task &t, const std::string &_function, const std::string &file,
                           int line, TASK_PRIORITY p)
    : task_wrapper(t, _function, file, line) {
  this->priority = p;
}

CONTINUATION_STRATEGY task_wrapper::apply(const thread_info &ti) {
  _tinfo.kind = ti.kind;
  _tinfo.thread_number = ti.thread_number;

  if (worker() == CONTINUATION_STRATEGY::SINGLE) {
    _result->unlock();
    return CONTINUATION_STRATEGY::SINGLE;
  }

  return CONTINUATION_STRATEGY::REPEAT;
}

CONTINUATION_STRATEGY task_wrapper::worker() {
  try {
    return _task(this->_tinfo);
  } catch (std::exception &ex) {
    logger_fatal("utils: *** async task exception:", _parent_function,
                 " file:", _code_file, " line:", _code_line);
    logger_fatal("utils: *** what:", ex.what());
    throw;
  }
}

task_result_ptr task_wrapper::result() const {
  return _result;
}