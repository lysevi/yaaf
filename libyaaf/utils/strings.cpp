#include <libyaaf/utils/strings.h>
#include <algorithm>
#include <clocale>
#include <ctype.h>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

std::vector<std::string> yaaf::utils::strings::tokens(const std::string &str) {
  std::vector<std::string> tokens;
  std::istringstream iss(str);
  std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(),
            std::back_inserter(tokens));
  return tokens;
}

std::vector<std::string> yaaf::utils::strings::split(const std::string &text, char sep) {
  auto n = std::count_if(text.begin(), text.end(), [sep](auto c) { return c == sep; });

  std::vector<std::string> tokens;
  tokens.reserve(n);

  std::size_t start = 0, end = 0;
  while ((end = text.find(sep, start)) != std::string::npos) {
    std::string temp = text.substr(start, end - start);
    if (temp != "") {
      tokens.emplace_back(std::move(temp));
    }
    start = end + 1;
  }
  std::string temp = text.substr(start);
  if (temp != "") {
    tokens.emplace_back(std::move(temp));
  }
  return tokens;
}

std::string yaaf::utils::strings::to_upper(const std::string &text) {
  std::string converted = text;

  std::transform(std::begin(text), std::end(text), std::begin(converted),
                 [](auto c) { return (char)toupper(c); });

  return converted;
}

std::string yaaf::utils::strings::to_lower(const std::string &text) {
  std::string converted = text;

  std::transform(std::begin(text), std::end(text), std::begin(converted),
                 [](auto c) { return (char)tolower(c); });
  return converted;
}

std::string yaaf::utils::strings::inner::to_string(const char *_Val) {
  return std::string(_Val);
}

std::string yaaf::utils::strings::inner::to_string(const std::string &_Val) {
  return _Val;
}
