#pragma once

#include <libyaaf/exports.h>
#include <libyaaf/serialization/helpers.h>
#include <libyaaf/utils/utils.h>
#include <cstddef>

namespace yaaf {
namespace serialization {

template <class... T> class binary_io {

public:
  static size_t capacity(const T &... args) noexcept {
    size_t result = 0;
    helpers::calculate_args_size(result, std::forward<const T>(args)...);
    return result;
  }

  static size_t capacity(T &&... args) noexcept {
    size_t result = 0;
    helpers::calculate_args_size(result, std::move(args)...);
    return result;
  }

  static void write(uint8_t *it, const T &... t) noexcept {
    helpers::write_args(it, std::forward<const T>(t)...);
  }

  static void write(uint8_t *it, T &&... t) noexcept {
    helpers::write_args(it, std::move(t)...);
  }

  static void read(uint8_t *it, T &... t) noexcept { helpers::read_args(it, t...); }
};

template <typename T> struct object_packer {
  static size_t capacity(const T &t) noexcept {
    return binary_io<T>::capacity(t);
  }

  static size_t capacity(T &&t) noexcept {
    return binary_io<T>::capacity(std::move(t));
  }

  template <class Iterator> static void pack(Iterator it, const T &t) noexcept {
    return binary_io<T>::write(it, t);
  }

  template <class Iterator> static void pack(Iterator it, T &&t) noexcept {
    return binary_io<T>::write(it, std::move(t));
  }

  static T unpack(uint8_t *it) noexcept {
    T result = empty();
    binary_io<T>::read(it, result);
    return result;
  }
  static T empty() {
    static_assert(std::is_default_constructible<T>::value, "T is default_constructible");
    return T{};
  }
};

} // namespace serialization
} // namespace yaaf
