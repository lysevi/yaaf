#include "helpers.h"
#include <libyaaf/network/queries.h>
#include <libyaaf/serialization/serialization.h>
#include <libyaaf/utils/utils.h>
#include <algorithm>

#include <catch.hpp>

using namespace yaaf;
using namespace yaaf::utils;
using namespace yaaf::network;
using namespace yaaf::network::queries;

TEST_CASE("serialization.ok") {
  ok qok{std::numeric_limits<uint64_t>::max()};
  auto nd = qok.get_message();
  EXPECT_EQ(nd->get_header()->kind, (network::message::kind_t)messagekinds::OK);

  auto repacked = ok(nd);
  EXPECT_EQ(repacked.id, qok.id);
}

TEST_CASE("serialization.login") {
  login lg{"login"};
  auto nd = lg.get_message();
  EXPECT_EQ(nd->get_header()->kind, (network::message::kind_t)messagekinds::LOGIN);

  auto repacked = login(nd);
  EXPECT_EQ(repacked.login_str, lg.login_str);
}

TEST_CASE("serialization.login_confirm") {
  login_confirm lg{uint64_t(1)};
  auto nd = lg.get_message();
  EXPECT_EQ(nd->get_header()->kind, (network::message::kind_t)messagekinds::LOGIN_CONFIRM);

  auto repacked = login_confirm(nd);
  EXPECT_EQ(repacked.id, lg.id);
}

TEST_CASE("serialization.size_of_args") {
  EXPECT_EQ(serialization::binary_io<int>::capacity(int(1)), sizeof(int));
  auto sz = serialization::binary_io<int, int>::capacity(int(1), int(1));
  EXPECT_EQ(sz, sizeof(int) * 2);

  sz = serialization::binary_io<int, int, double>::capacity(int(1), int(1),
                                                                     double(1.0));
  EXPECT_EQ(sz, sizeof(int) * 2 + sizeof(double));

  std::string str = "hello world";
  sz = serialization::binary_io<std::string>::capacity(std::move(str));
  EXPECT_EQ(sz, sizeof(uint32_t) + str.size());
}

TEST_CASE("serialization.scheme") {
  std::vector<uint8_t> buffer(1024);

  auto it = buffer.data();
  serialization::binary_io<int, int>::write(it, 1, 2);

  it = buffer.data();
  int unpacked1, unpacked2;

  serialization::binary_io<int, int>::read(it, unpacked1, unpacked2);
  EXPECT_EQ(unpacked1, 1);
  EXPECT_EQ(unpacked2, 2);

  it = buffer.data();
  std::string str = "hello world";
  serialization::binary_io<int, std::string>::write(it, 11, std::move(str));

  it = buffer.data();
  std::string unpackedS;
  serialization::binary_io<int, std::string>::read(it, unpacked1, unpackedS);
  EXPECT_EQ(unpacked1, 11);
  EXPECT_EQ(unpackedS, str);
}

struct SchemeTestObject {
  uint64_t id;
  std::string login;
};

namespace yaaf {
namespace serialization {
template <> struct object_packer<SchemeTestObject> {
  using Scheme = yaaf::serialization::binary_io<uint64_t, std::string>;

  static size_t capacity(const SchemeTestObject &t) {
    return Scheme::capacity(t.id, t.login);
  }
  template <class Iterator> static void pack(Iterator it, const SchemeTestObject t) {
    return Scheme::write(it, t.id, t.login);
  }
  template <class Iterator> static SchemeTestObject unpack(Iterator ii) {
    SchemeTestObject t{};
    Scheme::read(ii, t.id, t.login);
    return t;
  }
};
} // namespace serialization
} // namespace yaaf

TEST_CASE("serialization.objectscheme") {
  SchemeTestObject ok{std::numeric_limits<uint64_t>::max(), std::string("test_login")};

  network::message::size_t neededSize = static_cast<network::message::size_t>(
      yaaf::serialization::object_packer<SchemeTestObject>::capacity(ok));

  auto nd = std::make_shared<network::message>(
      neededSize, (network::message::kind_t)messagekinds::LOGIN);

  yaaf::serialization::object_packer<SchemeTestObject>::pack(nd->value(), ok);

  auto repacked = yaaf::serialization::object_packer<SchemeTestObject>::unpack(nd->value());
  EXPECT_EQ(repacked.id, ok.id);
  EXPECT_EQ(repacked.login, ok.login);
}

TEST_CASE("serialization.message") {
  SchemeTestObject msg_inner{std::numeric_limits<uint64_t>::max(),
                             std::string("test_login")};

  queries::packed_message<SchemeTestObject> lg{uint64_t(1), yaaf::id_t(1), yaaf::id_t(2), msg_inner};
  auto nd = lg.get_message();
  EXPECT_EQ(nd->get_header()->kind, (network::message::kind_t)messagekinds::MSG);

  auto repacked = queries::packed_message<SchemeTestObject>(nd);
  EXPECT_EQ(repacked.id, lg.id);
  EXPECT_EQ(repacked.asyncOperationid, uint64_t(1));
  EXPECT_EQ(repacked.clientid, uint64_t(2));

  EXPECT_EQ(repacked.msg.id, msg_inner.id);
  EXPECT_EQ(repacked.msg.login, msg_inner.login);
}