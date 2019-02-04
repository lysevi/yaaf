#pragma once

#include <libnmq/exports.h>
#include <string>
#include <vector>

namespace nmq {
namespace serialization {
namespace helpers {

/// sizeof

template <class T> std::enable_if_t<std::is_pod_v<T>, size_t> size(const T &) noexcept {
  return sizeof(T);
}

inline size_t size(const std::string &s) noexcept {
  return sizeof(uint32_t) + s.length();
}

template <class T> size_t size(const std::vector<T &&> &s) noexcept {
  static_assert(std::is_pod<T>::value, "T is not a POD object");
  return sizeof(uint32_t) + s.size() * sizeof(T);
}

/// write

template <typename S>
std::enable_if_t<std::is_pod_v<S>, size_t> write(uint8_t *it, const S &s) noexcept {
  std::memcpy(it, &s, sizeof(s));
  return sizeof(s);
}

inline size_t write(uint8_t *it, const std::string &s) noexcept {
  auto len = static_cast<uint32_t>(s.size());
  auto ptr = it;
  std::memcpy(ptr, &len, sizeof(uint32_t));
  std::memcpy(ptr + sizeof(uint32_t), s.data(), s.size());
  return sizeof(uint32_t) + s.size();
}

template <class T> size_t write(uint8_t *it, const std::vector<T &&> &s) noexcept {
  static_assert(std::is_pod<T>::value, "T is not a POD object");
  auto len = static_cast<uint32_t>(s.size());
  auto ptr = it;
  std::memcpy(ptr, &len, sizeof(uint32_t));
  std::memcpy(ptr + sizeof(uint32_t), s.data(), s.size() * sizeof(T));
  return sizeof(uint32_t) + s.size() * sizeof(T);
}

/// Read

template <typename S>
std::enable_if_t<std::is_pod_v<S>, size_t> read(uint8_t *it, S &s) noexcept {
  auto ptr = it;
  std::memcpy(&s, ptr, sizeof(s));
  return sizeof(s);
}

inline static size_t read(uint8_t *it, std::string &s) noexcept {
  uint32_t len = 0;
  auto ptr = it;
  std::memcpy(&len, ptr, sizeof(uint32_t));
  s.resize(len);
  std::memcpy(&s[0], ptr + sizeof(uint32_t), size_t(len));
  return sizeof(uint32_t) + len;
}
template <class T> size_t read(uint8_t *it, std::vector<T &&> &s) noexcept {
  static_assert(std::is_pod<T>::value, "S is not a POD value");
  uint32_t len = 0;
  auto ptr = it;
  std::memcpy(&len, ptr, sizeof(uint32_t));
  s.resize(len);
  std::memcpy(&s[0], ptr + sizeof(uint32_t), size_t(len) * sizeof(T));
  return sizeof(uint32_t) + len * sizeof(T);
}

/// Recursive
template <typename Head> static void calculate_args_size(size_t &result, Head &&head) noexcept {
  result += helpers::size(head);
}

template <typename Head, typename... Tail>
static void calculate_args_size(size_t &result, Head &&head, Tail &&... t) noexcept {
  result += helpers::size(std::forward<Head>(head));
  calculate_args_size(result, std::forward<Tail>(t)...);
}

template <typename Head> static void write_args(uint8_t *it, Head &&head) noexcept {
  auto szofcur = helpers::write(it, head);
  it += szofcur;
}

template <typename Head, typename... Tail>
static void write_args(uint8_t *it, Head &&head, Tail &&... t) noexcept {
  auto szofcur = helpers::write(it, head);
  it += szofcur;
  write_args(it, std::forward<Tail>(t)...);
}

template <typename Head> static void read_args(uint8_t *it, Head &head) noexcept {
  auto szofcur = helpers::read(it, head);
  it += szofcur;
}

template <typename Head, typename... Tail>
static void read_args(uint8_t *it, Head &head, Tail &... t) noexcept {
  auto szofcur = helpers::read(it, head);
  it += szofcur;
  read_args(it, t...);
}

} // namespace helpers
} // namespace serialization
} // namespace nmq
