#pragma once
#if YAAF_NETWORK_ENABLED
#include <libyaaf/network/dialler.h>
#include <libyaaf/network/listener.h>
#include <libyaaf/serialization/serialization.h>

namespace yaaf {

struct network_actor_message {
  std::string name;
  std::vector<unsigned char> data;
};

struct listener_actor_message {
  uint64_t sender_id;
  std::string name;
  std::vector<unsigned char> data;
};

struct connection_status_message {
  std::string host;
  std::string errormsg;
  bool is_connected = false;
};

template <> struct serialization::object_packer<network_actor_message> {
  using Scheme = yaaf::serialization::binary_io<std::string, std::vector<unsigned char>>;

  static size_t capacity(const network_actor_message &t) {
    return Scheme::capacity(t.name, t.data);
  }

  template <class Iterator> static void pack(Iterator it, const network_actor_message t) {
    Scheme::write(it, t.name, t.data);
  }

  template <class Iterator> static network_actor_message unpack(Iterator ii) {
    network_actor_message t{};
    Scheme::read(ii, t.name, t.data);
    return t;
  }
};
} // namespace yaaf

#endif