#include "helpers.h"
#include <libnmq/network/queries.h>
#include <libnmq/serialization/serialization.h>
#include <libnmq/utils/utils.h>
#include <algorithm>

#include <catch.hpp>

using namespace nmq;
using namespace nmq::utils;
using namespace nmq::network;
using namespace nmq::network::queries;

TEST_CASE("serialization.ok") {
  Ok ok{std::numeric_limits<uint64_t>::max()};
  auto nd = ok.getMessage();
  EXPECT_EQ(nd->header()->kind, (network::Message::kind_t)MessageKinds::OK);

  auto repacked = Ok(nd);
  EXPECT_EQ(repacked.id, ok.id);
}

TEST_CASE("serialization.login") {
  Login lg{"login"};
  auto nd = lg.getMessage();
  EXPECT_EQ(nd->header()->kind, (network::Message::kind_t)MessageKinds::LOGIN);

  auto repacked = Login(nd);
  EXPECT_EQ(repacked.login, lg.login);
}

TEST_CASE("serialization.login_confirm") {
  LoginConfirm lg{uint64_t(1)};
  auto nd = lg.getMessage();
  EXPECT_EQ(nd->header()->kind,
            (network::Message::kind_t)MessageKinds::LOGIN_CONFIRM);

  auto repacked = LoginConfirm(nd);
  EXPECT_EQ(repacked.id, lg.id);
}

TEST_CASE("serialization.size_of_args") {
  EXPECT_EQ(serialization::Scheme<int>::capacity(int(1)), sizeof(int));
  auto sz = serialization::Scheme<int, int>::capacity(int(1), int(1));
  EXPECT_EQ(sz, sizeof(int) * 2);

  sz = serialization::Scheme<int, int, double>::capacity(int(1), int(1), double(1.0));
  EXPECT_EQ(sz, sizeof(int) * 2 + sizeof(double));

  std::string str = "hello world";
  sz = serialization::Scheme<std::string>::capacity(std::move(str));
  EXPECT_EQ(sz, sizeof(uint32_t) + str.size());
}

TEST_CASE("serialization.scheme") {
  std::vector<int8_t> buffer(1024);

  auto it = buffer.begin();
  serialization::Scheme<int, int>::write(it, 1, 2);

  it = buffer.begin();
  int unpacked1, unpacked2;

  serialization::Scheme<int, int>::read(it, unpacked1, unpacked2);
  EXPECT_EQ(unpacked1, 1);
  EXPECT_EQ(unpacked2, 2);

  it = buffer.begin();
  std::string str = "hello world";
  serialization::Scheme<int, std::string>::write(it, 11, std::move(str));

  it = buffer.begin();
  std::string unpackedS;
  serialization::Scheme<int, std::string>::read(it, unpacked1, unpackedS);
  EXPECT_EQ(unpacked1, 11);
  EXPECT_EQ(unpackedS, str);
}

struct SchemeTestObject {
  uint64_t id;
  std::string login;
};

namespace nmq {
namespace serialization {
template <> struct ObjectScheme<SchemeTestObject> {
  using Scheme = nmq::serialization::Scheme<uint64_t, std::string>;

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
} // namespace nmq

TEST_CASE("serialization.objectscheme") {
  SchemeTestObject ok{std::numeric_limits<uint64_t>::max(), std::string("test_login")};

  network::Message::size_t neededSize = static_cast<network::Message::size_t>(
      nmq::serialization::ObjectScheme<SchemeTestObject>::capacity(ok));

  auto nd = std::make_shared<network::Message>(
      neededSize, (network::Message::kind_t)MessageKinds::LOGIN);

  nmq::serialization::ObjectScheme<SchemeTestObject>::pack(nd->value(), ok);

  auto repacked = nmq::serialization::ObjectScheme<SchemeTestObject>::unpack(nd->value());
  EXPECT_EQ(repacked.id, ok.id);
  EXPECT_EQ(repacked.login, ok.login);
}

TEST_CASE("serialization.message") {
  SchemeTestObject msg_inner{std::numeric_limits<uint64_t>::max(),
                             std::string("test_login")};

  queries::Message<SchemeTestObject> lg{uint64_t(1), msg_inner};
  auto nd = lg.getMessage();
  EXPECT_EQ(nd->header()->kind, (network::Message::kind_t)MessageKinds::MSG);

  auto repacked = queries::Message<SchemeTestObject>(nd);
  EXPECT_EQ(repacked.id, lg.id);

  EXPECT_EQ(repacked.msg.id, msg_inner.id);
  EXPECT_EQ(repacked.msg.login, msg_inner.login);
}