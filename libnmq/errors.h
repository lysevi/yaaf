#pragma once

#include <boost/system/error_code.hpp>

namespace nmq {
enum class errors_kinds { 
	ALL_LISTENERS_STOPED, 
	FULL_STOP,
	Ok };

struct ecode {
  ecode(boost::system::error_code e) : error(e), inner_error(errors_kinds::Ok) {}

  ecode(errors_kinds e) : inner_error(e) {}
  boost::system::error_code error;
  errors_kinds inner_error;
};

} // namespace nmq