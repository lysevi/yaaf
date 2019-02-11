#pragma once

#include <libyaaf/exports.h>
#include <libyaaf/types.h>

namespace yaaf {

struct payload_t {
private:
  struct base_holder {
    virtual ~base_holder() {}
    virtual const std::type_info &type_info() const = 0;
    virtual std::unique_ptr<base_holder> clone() const = 0;
  };

  template <typename T> struct holder : base_holder {
    holder(const T &t) : t_(t) {}
    const std::type_info &type_info() const override { return typeid(t_); }

    std::unique_ptr<base_holder> clone() const override {
      return std::make_unique<holder<T>>(t_);
    }
    T t_;
  };

public:
  template <typename T> payload_t(const T &t) : _holder(std::make_unique<holder<T>>(t)) {}
  payload_t() : _holder() {}
  payload_t(const payload_t &other) : _holder(other._holder->clone()) {}
  payload_t(payload_t &&other) : _holder(std::move(other._holder)) {
    other._holder = nullptr;
  }
  ~payload_t() {}

  template <typename U> U cast() const {
    if (typeid(U) != _holder->type_info()) {
      throw std::runtime_error("Bad any cast");
    }
    return static_cast<holder<U> *>(_holder.get())->t_;
  }

  bool empty() const { return _holder == nullptr; }

  template <typename U> bool is() const { return typeid(U) == _holder->type_info(); }

  friend void swap(payload_t &left, payload_t &right);

  payload_t &operator=(const payload_t &other) {
    payload_t tmp(other);
    swap(*this, tmp);
    return *this;
  }

private:
  std::unique_ptr<base_holder> _holder;
};

inline void swap(payload_t &left, payload_t &right) {
  using std::swap;
  if (&left != &right) {
    swap(left._holder, right._holder);
  }
}

} // namespace yaaf
