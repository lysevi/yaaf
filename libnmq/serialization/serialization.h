#pragma once

#include <libnmq/exports.h>
#include <cstddef>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace nmq {
namespace serialization {

template <class T> struct get_size {
  static size_t size(const T &) { return sizeof(T); }
};

template <> struct get_size<std::string> {
  static size_t size(const std::string &s) { return sizeof(uint32_t) + s.length(); }
};

template <class T> struct get_size<std::vector<T>> {
  static size_t size(const std::vector<T> &s) {
    static_assert(std::is_pod<T>::value, "T is not a POD object");
    return sizeof(uint32_t) + s.size() * sizeof(T);
  }
};

template <typename Iterator, typename S> struct writer {
  static size_t write_value(Iterator it, const S &s) {
    static_assert(std::is_pod<S>::value, "S is not a POD value");
    std::memcpy(&(*it), &s, sizeof(s));
    return sizeof(s);
  }
};

template <typename Iterator> struct writer<Iterator, std::string> {
  static size_t write_value(Iterator it, const std::string &s) {
    auto len = static_cast<uint32_t>(s.size());
    auto ptr = &(*it);
    std::memcpy(ptr, &len, sizeof(uint32_t));
    std::memcpy(ptr + sizeof(uint32_t), s.data(), s.size());
    return sizeof(uint32_t) + s.size();
  }
};

template <typename Iterator, class T> struct writer<Iterator, std::vector<T>> {
  static size_t write_value(Iterator it, const std::vector<T> &s) {
    static_assert(std::is_pod<T>::value, "T is not a POD object");
    auto len = static_cast<uint32_t>(s.size());
    auto ptr = &(*it);
    std::memcpy(ptr, &len, sizeof(uint32_t));
    std::memcpy(ptr + sizeof(uint32_t), s.data(), s.size() * sizeof(T));
    return sizeof(uint32_t) + s.size() * sizeof(T);
  }
};

template <typename Iterator, typename S> struct reader {
  static size_t read_value(Iterator it, S &s) {
    static_assert(std::is_pod<S>::value, "S is not a POD value");
    auto ptr = &(*it);
    std::memcpy(&s, ptr, sizeof(s));
    return sizeof(s);
  }
};

template <typename Iterator> struct reader<Iterator, std::string> {
  static size_t read_value(Iterator it, std::string &s) {
    uint32_t len = 0;
    auto ptr = &(*it);
    std::memcpy(&len, ptr, sizeof(uint32_t));
    s.resize(len);
    std::memcpy(&s[0], ptr + sizeof(uint32_t), size_t(len));
    return sizeof(uint32_t) + len;
  }
};

template <typename Iterator, class T> struct reader<Iterator, std::vector<T>> {
  static size_t read_value(Iterator it, std::vector<T> &s) {
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
  static void calculate_size_rec(size_t &result, const Head &&head) {
    result += get_size<Head>::size(head);
  }

  template <typename Head, typename... Tail>
  static void calculate_size_rec(size_t &result, const Head &&head, const Tail &&... t) {
    result += get_size<Head>::size(std::forward<const Head>(head));
    calculate_size_rec(result, std::forward<const Tail>(t)...);
  }

  template <class Iterator, typename Head>
  static void write_args(Iterator it, const Head &head) {
    auto szofcur = writer<Iterator, Head>::write_value(it, head);
    it += szofcur;
  }

  template <class Iterator, typename Head, typename... Tail>
  static void write_args(Iterator it, const Head &head, const Tail &... t) {
    auto szofcur = writer<Iterator, Head>::write_value(it, head);
    it += szofcur;
    write_args(it, std::forward<const Tail>(t)...);
  }

  template <class Iterator, typename Head>
  static void read_args(Iterator it, Head &&head) {
    auto szofcur = reader<Iterator, Head>::read_value(it, head);
    it += szofcur;
  }

  template <class Iterator, typename Head, typename... Tail>
  static void read_args(Iterator it, Head &&head, Tail &&... t) {
    auto szofcur = reader<Iterator, Head>::read_value(it, head);
    it += szofcur;
    read_args(it, std::forward<Tail>(t)...);
  }

public:
  static size_t capacity(const T &... args) {
    size_t result = 0;
    calculate_size_rec(result, std::forward<const T>(args)...);
    return result;
  }

  template <class Iterator> static void write(Iterator it, const T &... t) {
    write_args(it, std::forward<const T>(t)...);
  }

  template <class Iterator> static void read(Iterator it, T &... t) {
    read_args(it, std::forward<T>(t)...);
  }
};
} // namespace serialization
} // namespace nmq
