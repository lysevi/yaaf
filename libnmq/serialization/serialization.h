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

template <typename Iterator, typename S> struct Writer {
  static size_t write(Iterator it, const S &s) {
    static_assert(std::is_pod<S>::value, "S is not a POD value");
    std::memcpy(&(*it), &s, sizeof(s));
    return sizeof(s);
  }
};

template <typename Iterator> struct Writer<Iterator, std::string> {
  static size_t write(Iterator it, const std::string &s) {
    auto len = static_cast<uint32_t>(s.size());
    auto ptr = &(*it);
    std::memcpy(ptr, &len, sizeof(uint32_t));
    std::memcpy(ptr + sizeof(uint32_t), s.data(), s.size());
    return sizeof(uint32_t) + s.size();
  }
};

template <typename Iterator, class T> struct Writer<Iterator, std::vector<T>> {
  static size_t write(Iterator it, const std::vector<T> &s) {
    static_assert(std::is_pod<T>::value, "T is not a POD object");
    auto len = static_cast<uint32_t>(s.size());
    auto ptr = &(*it);
    std::memcpy(ptr, &len, sizeof(uint32_t));
    std::memcpy(ptr + sizeof(uint32_t), s.data(), s.size() * sizeof(T));
    return sizeof(uint32_t) + s.size() * sizeof(T);
  }
};

template <typename Iterator, typename S> struct Reader {
  static size_t read(Iterator it, S &s) {
    static_assert(std::is_pod<S>::value, "S is not a POD value");
    auto ptr = &(*it);
    std::memcpy(&s, ptr, sizeof(s));
    return sizeof(s);
  }
};

template <typename Iterator> struct Reader<Iterator, std::string> {
  static size_t read(Iterator it, std::string &s) {
    uint32_t len = 0;
    auto ptr = &(*it);
    std::memcpy(&len, ptr, sizeof(uint32_t));
    s.resize(len);
    std::memcpy(&s[0], ptr + sizeof(uint32_t), size_t(len));
    return sizeof(uint32_t) + len;
  }
};

template <typename Iterator, class T> struct Reader<Iterator, std::vector<T>> {
  static size_t read(Iterator it, std::vector<T> &s) {
    static_assert(std::is_pod<T>::value, "S is not a POD value");
    uint32_t len = 0;
    auto ptr = &(*it);
    std::memcpy(&len, ptr, sizeof(uint32_t));
    s.resize(len);
    std::memcpy(&s[0], ptr + sizeof(uint32_t), size_t(len) * sizeof(T));
    return sizeof(uint32_t) + len * sizeof(T);
  }
};

template <class... T> class Scheme {
  template <typename Head>
  static void calculateSize(size_t &result, const Head &&head) {
    result += getSize<Head>::size(head);
  }

  template <typename Head, typename... Tail>
  static void calculateSize(size_t &result, const Head &&head, const Tail &&... t) {
    result += getSize<Head>::size(std::forward<const Head>(head));
    calculateSize(result, std::forward<const Tail>(t)...);
  }

  template <class Iterator, typename Head>
  static void writeArgs(Iterator it, const Head &head) {
    auto szofcur = Writer<Iterator, Head>::write(it, head);
    it += szofcur;
  }

  template <class Iterator, typename Head, typename... Tail>
  static void writeArgs(Iterator it, const Head &head, const Tail &... t) {
    auto szofcur = Writer<Iterator, Head>::write(it, head);
    it += szofcur;
    writeArgs(it, std::forward<const Tail>(t)...);
  }

  template <class Iterator, typename Head>
  static void readArgs(Iterator it, Head &&head) {
    auto szofcur = Reader<Iterator, Head>::read(it, head);
    it += szofcur;
  }

  template <class Iterator, typename Head, typename... Tail>
  static void readArgs(Iterator it, Head &&head, Tail &&... t) {
    auto szofcur = Reader<Iterator, Head>::read(it, head);
    it += szofcur;
    readArgs(it, std::forward<Tail>(t)...);
  }

public:
  static size_t capacity(const T &... args) {
    size_t result = 0;
    calculateSize(result, std::forward<const T>(args)...);
    return result;
  }

  template <class Iterator> static void write(Iterator it, const T &... t) {
    writeArgs(it, std::forward<const T>(t)...);
  }

  template <class Iterator> static void read(Iterator it, T &... t) {
    readArgs(it, std::forward<T>(t)...);
  }
};

template <typename T> struct ObjectScheme {
  static size_t capacity(const T &t) {

    NOT_IMPLEMENTED;
    return 0;
  }
  template <class Iterator> static void pack(Iterator it, const T t) { NOT_IMPLEMENTED; }
  template <class Iterator> static T unpack(Iterator ii) { NOT_IMPLEMENTED; }
  static T empty() { return T(); }
};

} // namespace serialization
} // namespace nmq
