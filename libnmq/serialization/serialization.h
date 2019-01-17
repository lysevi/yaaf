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

template <class T> struct getSize {
  static size_t size(const T &) { return sizeof(T); }
};

template <> struct getSize<std::string> {
  static size_t size(const std::string &s) { return sizeof(uint32_t) + s.length(); }
};

template <class T> struct getSize<std::vector<T>> {
  static size_t size(const std::vector<T> &s) {
    static_assert(std::is_pod<T>::value, "T is not a POD object");
    return sizeof(uint32_t) + s.size() * sizeof(T);
  }
};

template <typename S> struct Writer {
  static size_t write(uint8_t *it, const S &s) {
    static_assert(std::is_pod<S>::value, "S is not a POD value");
    std::memcpy(it, &s, sizeof(s));
    return sizeof(s);
  }
};

template <> struct Writer<std::string> {
  static size_t write(uint8_t *it, const std::string &s) {
    auto len = static_cast<uint32_t>(s.size());
    auto ptr = it;
    std::memcpy(ptr, &len, sizeof(uint32_t));
    std::memcpy(ptr + sizeof(uint32_t), s.data(), s.size());
    return sizeof(uint32_t) + s.size();
  }
};

template <class T> struct Writer<std::vector<T>> {
  static size_t write(uint8_t *it, const std::vector<T> &s) {
    static_assert(std::is_pod<T>::value, "T is not a POD object");
    auto len = static_cast<uint32_t>(s.size());
    auto ptr = it;
    std::memcpy(ptr, &len, sizeof(uint32_t));
    std::memcpy(ptr + sizeof(uint32_t), s.data(), s.size() * sizeof(T));
    return sizeof(uint32_t) + s.size() * sizeof(T);
  }
};

template <typename S> struct Reader {
  static size_t read(uint8_t *it, S &s) {
    static_assert(std::is_pod<S>::value, "S is not a POD value");
    auto ptr = it;
    std::memcpy(&s, ptr, sizeof(s));
    return sizeof(s);
  }
};

template <> struct Reader<std::string> {
  static size_t read(uint8_t *it, std::string &s) {
    uint32_t len = 0;
    auto ptr = it;
    std::memcpy(&len, ptr, sizeof(uint32_t));
    s.resize(len);
    std::memcpy(&s[0], ptr + sizeof(uint32_t), size_t(len));
    return sizeof(uint32_t) + len;
  }
};

template <class T> struct Reader<std::vector<T>> {
  static size_t read(uint8_t *it, std::vector<T> &s) {
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
  template <typename Head> static void calculateSize(size_t &result, const Head &&head) {
    result += getSize<Head>::size(head);
  }

  template <typename Head, typename... Tail>
  static void calculateSize(size_t &result, const Head &&head, const Tail &&... t) {
    result += getSize<Head>::size(std::forward<const Head>(head));
    calculateSize(result, std::forward<const Tail>(t)...);
  }

  template <typename Head> static void writeArgs(uint8_t *it, const Head &head) {
    auto szofcur = Writer<Head>::write(it, head);
    it += szofcur;
  }

  template <typename Head, typename... Tail>
  static void writeArgs(uint8_t *it, const Head &head, const Tail &... t) {
    auto szofcur = Writer<Head>::write(it, head);
    it += szofcur;
    writeArgs(it, std::forward<const Tail>(t)...);
  }

  template <typename Head> static void readArgs(uint8_t *it, Head &&head) {
    auto szofcur = Reader<Head>::read(it, head);
    it += szofcur;
  }

  template <typename Head, typename... Tail>
  static void readArgs(uint8_t *it, Head &&head, Tail &&... t) {
    auto szofcur = Reader<Head>::read(it, head);
    it += szofcur;
    readArgs(it, std::forward<Tail>(t)...);
  }

public:
  static size_t capacity(const T &... args) {
    size_t result = 0;
    calculateSize(result, std::forward<const T>(args)...);
    return result;
  }

  static void write(uint8_t *it, const T &... t) {
    writeArgs(it, std::forward<const T>(t)...);
  }

  static void read(uint8_t *it, T &... t) { readArgs(it, std::forward<T>(t)...); }
};

template <typename T> struct ObjectScheme {
  static size_t capacity(const T &t) { return BinaryReaderWriter<T>::capacity(t); }
  template <class Iterator> static void pack(Iterator it, const T t) {
    return BinaryReaderWriter<T>::write(it, t);
  }
  static T unpack(uint8_t *ii) {
    T result = empty();
    BinaryReaderWriter<T>::read(it, t);
    return result;
  }
  static T empty() {
    static_cast(std::is_default_constructible<T>::value, "T is default_constructible");
    return T{};
  }
};

} // namespace serialization
} // namespace nmq
