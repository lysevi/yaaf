#pragma once

#include <libyaaf/exports.h>
#include <string>
#include <vector>

namespace yaaf {
namespace utils {
namespace strings {

/// split string by space.
EXPORT std::vector<std::string> tokens(const std::string &str);
EXPORT std::vector<std::string> split(const std::string &text, char sep);
EXPORT std::string to_upper(const std::string &text);
EXPORT std::string to_lower(const std::string &text);

namespace inner {
using std::to_string;
EXPORT std::string to_string(const char *_Val);
EXPORT std::string to_string(const std::string &_Val);

template <size_t N, class Head>
void args_as_string(std::string (&s)[N], size_t pos, size_t &sz, Head &&head) noexcept {
  auto str = to_string(std::forward<Head>(head));
  sz += str.size();
  s[pos] = std::move(str);
}

template <size_t N, class Head, class... Tail>
void args_as_string(std::string (&s)[N], size_t pos, size_t &sz, Head &&head,
                    Tail &&... tail) noexcept {
  auto str = to_string(std::forward<Head>(head));
  sz += str.size();
  s[pos] = std::move(str);
  args_as_string(s, pos + 1, sz, std::forward<Tail>(tail)...);
}
} // namespace inner

template <class... Args> std::string args_to_string(Args &&... args) noexcept {
  const size_t n = sizeof...(args);
  std::string ss[n];
  size_t sz = 0;
  inner::args_as_string(ss, size_t(0), sz, std::forward<Args>(args)...);
  std::string result;
  result.reserve(sz);
  for (auto &&v : ss) {
    result += std::move(v);
  }
  return result;
}
} // namespace strings
} // namespace utils
} // namespace yaaf
