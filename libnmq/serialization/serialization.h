#pragma once

#include <libnmq/exports.h>
#include <libnmq/utils/utils.h>
#include <cstddef>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace nmq {
namespace serialization {

struct getSize {
  template <class T>
  static std::enable_if_t<std::is_pod_v<T>, size_t> size(const T &) noexcept {
    return sizeof(T);
  }

  static size_t size(const std::string &s) noexcept {
    return sizeof(uint32_t) + s.length();
  }
  template <class T> static size_t size(const std::vector<T &&> &s) noexcept {
    static_assert(std::is_pod<T>::value, "T is not a POD object");
    return sizeof(uint32_t) + s.size() * sizeof(T);
  }
};

struct Writer {
  template <typename S>
  static std::enable_if_t<std::is_pod_v<S>, size_t> write(uint8_t *it, const S &s) {
    std::memcpy(it, &s, sizeof(s));
    return sizeof(s);
  }

  static size_t write(uint8_t *it, const std::string &s) {
    auto len = static_cast<uint32_t>(s.size());
    auto ptr = it;
    std::memcpy(ptr, &len, sizeof(uint32_t));
    std::memcpy(ptr + sizeof(uint32_t), s.data(), s.size());
    return sizeof(uint32_t) + s.size();
  }

  template <class T> static size_t write(uint8_t *it, const std::vector<T &&> &s) {
    static_assert(std::is_pod<T>::value, "T is not a POD object");
    auto len = static_cast<uint32_t>(s.size());
    auto ptr = it;
    std::memcpy(ptr, &len, sizeof(uint32_t));
    std::memcpy(ptr + sizeof(uint32_t), s.data(), s.size() * sizeof(T));
    return sizeof(uint32_t) + s.size() * sizeof(T);
  }
};

struct Reader {
  template <typename S>
  static std::enable_if_t<std::is_pod_v<S>, size_t> read(uint8_t *it, S &s) {
    auto ptr = it;
    std::memcpy(&s, ptr, sizeof(s));
    return sizeof(s);
  }

  static size_t read(uint8_t *it, std::string &s) {
    uint32_t len = 0;
    auto ptr = it;
    std::memcpy(&len, ptr, sizeof(uint32_t));
    s.resize(len);
    std::memcpy(&s[0], ptr + sizeof(uint32_t), size_t(len));
    return sizeof(uint32_t) + len;
  }
  template <class T> static size_t read(uint8_t *it, std::vector<T &&> &s) {
    static_assert(std::is_pod<T>::value, "S is not a POD value");
    uint32_t len = 0;
    auto ptr = it;
    std::memcpy(&len, ptr, sizeof(uint32_t));
    s.resize(len);
    std::memcpy(&s[0], ptr + sizeof(uint32_t), size_t(len) * sizeof(T));
    return sizeof(uint32_t) + len * sizeof(T);
  }
};

template <class... T> class BinaryReaderWriter {
  template <typename Head>
  static void calculateSize(size_t &result, Head &&head) noexcept {
    result += getSize::size(head);
  }

  template <typename Head, typename... Tail>
  static void calculateSize(size_t &result, Head &&head, Tail &&... t) noexcept {
    result += getSize::size(std::forward<Head>(head));
    calculateSize(result, std::forward<Tail>(t)...);
  }

  template <typename Head> static void writeArgs(uint8_t *it, Head &&head) noexcept {
    auto szofcur = Writer::write(it, head);
    it += szofcur;
  }

  template <typename Head, typename... Tail>
  static void writeArgs(uint8_t *it, Head &&head, Tail &&... t) noexcept {
    auto szofcur = Writer::write(it, head);
    it += szofcur;
    writeArgs(it, std::forward<Tail>(t)...);
  }

  template <typename Head> static void readArgs(uint8_t *it, Head &head) noexcept {
    auto szofcur = Reader::read(it, head);
    it += szofcur;
  }

  template <typename Head, typename... Tail>
  static void readArgs(uint8_t *it, Head &head, Tail &... t) noexcept {
    auto szofcur = Reader::read(it, head);
    it += szofcur;
    readArgs(it, t...);
  }

public:
  static size_t capacity(const T &... args) noexcept {
    size_t result = 0;
    calculateSize(result, std::forward<const T>(args)...);
    return result;
  }

  static size_t capacity(T &&... args) noexcept {
    size_t result = 0;
    calculateSize(result, std::move(args)...);
    return result;
  }

  static void write(uint8_t *it, const T &... t) noexcept {
    writeArgs(it, std::forward<const T>(t)...);
  }

  static void write(uint8_t *it, T &&... t) noexcept { writeArgs(it, std::move(t)...); }

  static void read(uint8_t *it, T &... t) noexcept { readArgs(it, t...); }
};

template <typename T> struct ObjectScheme {
  static size_t capacity(const T &t) noexcept {
    return BinaryReaderWriter<T>::capacity(t);
  }

  static size_t capacity(T &&t) noexcept {
    return BinaryReaderWriter<T>::capacity(std::move(t));
  }

  template <class Iterator> static void pack(Iterator it, const T &t) noexcept {
    return BinaryReaderWriter<T>::write(it, t);
  }

  template <class Iterator> static void pack(Iterator it, T &&t) noexcept {
    return BinaryReaderWriter<T>::write(it, std::move(t));
  }

  static T unpack(uint8_t *it) noexcept {
    T result = empty();
    BinaryReaderWriter<T>::read(it, result);
    return result;
  }
  static T empty() {
    static_assert(std::is_default_constructible<T>::value, "T is default_constructible");
    return T{};
  }
};

} // namespace serialization
} // namespace nmq
