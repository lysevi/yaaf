#pragma once

#include <boost/system/error_code.hpp>

namespace nmq {
enum class ErrorsKinds { 
	ALL_LISTENERS_STOPED, 
	FULL_STOP,
	Ok };

struct ErrorCode {
  ErrorCode(boost::system::error_code e) : error(e), inner_error(ErrorsKinds::Ok) {}

  ErrorCode(ErrorsKinds e) : inner_error(e) {}
  boost::system::error_code error;
  ErrorsKinds inner_error;
};

} // namespace nmq