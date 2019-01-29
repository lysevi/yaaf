#pragma once

#include <libnmq/exports.h>
#include <libnmq/serialization/helpers.h>
#include <libnmq/utils/utils.h>
#include <cstddef>

namespace nmq {
namespace serialization {

template <class... T> class BinaryReaderWriter {

public:
  static size_t capacity(const T &... args) noexcept {
    size_t result = 0;
    helpers::calculateSize(result, std::forward<const T>(args)...);
    return result;
  }

  static size_t capacity(T &&... args) noexcept {
    size_t result = 0;
    helpers::calculateSize(result, std::move(args)...);
    return result;
  }

  static void write(uint8_t *it, const T &... t) noexcept {
    helpers::writeArgs(it, std::forward<const T>(t)...);
  }

  static void write(uint8_t *it, T &&... t) noexcept {
    helpers::writeArgs(it, std::move(t)...);
  }

  static void read(uint8_t *it, T &... t) noexcept { helpers::readArgs(it, t...); }
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
