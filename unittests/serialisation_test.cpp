#include "helpers.h"
#include <libnmq/queries.h>
#include <libnmq/serialisation.h>
#include <libnmq/utils/utils.h>
#include <algorithm>

#include <catch.hpp>

using namespace nmq;
using namespace nmq::utils;
using namespace nmq::queries;

TEST_CASE("serialisation.ok") {
  Ok ok{std::numeric_limits<uint64_t>::max()};
  auto nd = ok.toNetworkMessage();
  EXPECT_EQ(nd->cast_to_header()->kind, (NetworkMessage::message_kind)MessageKinds::OK);

  auto repacked = Ok(nd);
  EXPECT_EQ(repacked.id, ok.id);
}

TEST_CASE("serialisation.login") {
  Login lg{"login"};
  auto nd = lg.toNetworkMessage();
  EXPECT_EQ(nd->cast_to_header()->kind,
            (NetworkMessage::message_kind)MessageKinds::LOGIN);

  auto repacked = Login(nd);
  EXPECT_EQ(repacked.login, lg.login);
}

TEST_CASE("serialisation.login_confirm") {
  LoginConfirm lg{uint64_t(1)};
  auto nd = lg.toNetworkMessage();
  EXPECT_EQ(nd->cast_to_header()->kind,
            (NetworkMessage::message_kind)MessageKinds::LOGIN_CONFIRM);

  auto repacked = LoginConfirm(nd);
  EXPECT_EQ(repacked.id, lg.id);
}

TEST_CASE("serialisation.size_of_args") {
  EXPECT_EQ(serialisation::Scheme<int>::capacity(int(1)), sizeof(int));
  auto sz = serialisation::Scheme<int, int>::capacity(int(1), int(1));
  EXPECT_EQ(sz, sizeof(int) * 2);

  sz = serialisation::Scheme<int, int, double>::capacity(int(1), int(1), double(1.0));
  EXPECT_EQ(sz, sizeof(int) * 2 + sizeof(double));

  std::string str = "hello world";
  sz = serialisation::Scheme<std::string>::capacity(std::move(str));
  EXPECT_EQ(sz, sizeof(uint32_t) + str.size());
}

TEST_CASE("serialisation.scheme") {
  std::vector<int8_t> buffer(1024);

  auto it = buffer.begin();
  serialisation::Scheme<int, int>::write(it, 1, 2);

  it = buffer.begin();
  int unpacked1, unpacked2;

  serialisation::Scheme<int, int>::read(it, unpacked1, unpacked2);
  EXPECT_EQ(unpacked1, 1);
  EXPECT_EQ(unpacked2, 2);

  it = buffer.begin();
  std::string str = "hello world";
  serialisation::Scheme<int, std::string>::write(it, 11, std::move(str));

  it = buffer.begin();
  std::string unpackedS;
  serialisation::Scheme<int, std::string>::read(it, unpacked1, unpackedS);
  EXPECT_EQ(unpacked1, 11);
  EXPECT_EQ(unpackedS, str);
}
