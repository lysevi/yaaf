#include "helpers.h"

#include <boost/asio.hpp>

#include <libnmq/network/connection.h>
#include <libnmq/network/listener.h>
#include <libnmq/network/transport.h>
#include <libnmq/utils/logger.h>

#include <catch.hpp>

#include <functional>
#include <string>
#include <thread>

using namespace std::placeholders;
using namespace boost::asio;

using namespace nmq;
using namespace nmq::utils;

struct MockMessage {
  uint64_t id;
  std::string msg;
};

struct MockResultMessage {
  uint64_t id;
  size_t length;
  std::string msg;
};

namespace nmq {
namespace serialization {
template <> struct ObjectScheme<MockMessage> {
  using BinaryRW = nmq::serialization::BinaryReaderWriter<uint64_t, std::string>;

  static size_t capacity(const MockMessage &t) { return BinaryRW::capacity(t.id, t.msg); }
  template <class Iterator> static void pack(Iterator it, const MockMessage t) {
    return BinaryRW::write(it, t.id, t.msg);
  }
  template <class Iterator> static MockMessage unpack(Iterator ii) {
    MockMessage t{};
    BinaryRW::read(ii, t.id, t.msg);
    return t;
  }
};

template <> struct ObjectScheme<MockResultMessage> {
  using BinaryRW = nmq::serialization::BinaryReaderWriter<uint64_t, size_t, std::string>;

  static size_t capacity(const MockResultMessage &t) {
    return BinaryRW::capacity(t.id, t.length, t.msg);
  }
  template <class Iterator> static void pack(Iterator it, const MockResultMessage t) {
    return BinaryRW::write(it, t.id, t.length, t.msg);
  }
  template <class Iterator> static MockResultMessage unpack(Iterator ii) {
    MockResultMessage t{};
    BinaryRW::read(ii, t.id, t.length, t.msg);
    return t;
  }
};
} // namespace serialization
} // namespace nmq

using MockTrasport = nmq::network::Transport<MockMessage, MockResultMessage>;

struct MockTransportListener : public MockTrasport::Listener {
  MockTransportListener(MockTrasport::Params &p) : MockTrasport::Listener(p.service, p) {}

  void onStartComplete() override { is_started_flag = true; }

  void onError(const MockTrasport::io_chanel_type::Sender &,
               const MockTrasport::io_chanel_type::ErrorCode &/*err*/) override {
    is_started_flag = false;
  };
  void onMessage(const MockTrasport::io_chanel_type::Sender &s, const MockMessage &d,
                 bool &) override {
    _q.insert(std::make_pair(d.id, d.msg));

    MockResultMessage answer;
    answer.id = d.id;
    answer.msg = d.msg + " " + d.msg;
    answer.length = answer.msg.size();

    if (this->isStopingBegin()) {
      return;
    }

    this->sendAsync(s.id, answer);
  }

  /**
  result - true for accept, false for failed.
  */
  bool onClient(const MockTrasport::io_chanel_type::Sender &) override { return true; }
  void onClientDisconnect(const MockTrasport::io_chanel_type::Sender &) override {}

  bool is_started_flag = false;
  std::map<uint64_t, std::string> _q;
};

struct MockTransportClient : public MockTrasport::Connection {
  MockTransportClient(const MockTrasport::Params &p, const std::string &login)
      : MockTrasport::Connection(p.service, login, p) {}

  void onConnected() override { is_started_flag = true; }

  void sendQuery() {
    MockMessage m;
    m.id = msg_id++;
    m.msg = "msg_" + std::to_string(m.id);
    this->sendAsync(m);
  }

  void onError(const MockTrasport::io_chanel_type::ErrorCode &/*er*/) override {
    is_started_flag = false;
  };
  void onMessage(const MockResultMessage &d, bool &) override {
    _q.insert(std::make_pair(d.id, d.length));
    sendQuery();
  }

  uint64_t msg_id = 1;
  bool is_started_flag = false;
  std::map<uint64_t, size_t> _q;
};

TEST_CASE("transport.network") {

  boost::asio::io_service transport_service;
  MockTrasport::Params p;
  p.service = &transport_service;
  p.host = "localhost";
  p.port = 4040;
  bool stop_flag = false;
  bool is_stoped_flag = false;
  bool is_started = false;

  auto srv_thread = [&]() {
    while (!stop_flag) {
      transport_service.run_one();
      is_started = true;
    }
    is_stoped_flag = true;
  };

  std::thread tr(srv_thread);

  while (!is_started) {
    logger("transport: !is_started");
    std::this_thread::yield();
  }

  auto listener = std::make_shared<MockTransportListener>(p);
  // auto connection = MockTrasport::connection(p, "c1");

  listener->start();
  // connection->start(client_on_data, client_on_error);
  while (!listener->is_started_flag) {
    logger("transport: !listener->is_started_flag");
    std::this_thread::yield();
  }

  auto client = std::make_shared<MockTransportClient>(p, "client");
  client->start();

  while (!client->is_started_flag) {
    logger("transport: !client->is_started_flag");
    std::this_thread::yield();
  }

  client->sendQuery();
  while (client->_q.size() < 10) {
    logger("transport: client->_q.size() < 10 :", client->_q.size());
    std::this_thread::yield();
  }

  listener->stop();

  while (client->is_started_flag) {
    logger("transport: client->is_started_flag");
    std::this_thread::yield();
  }
  stop_flag = true;
  tr.join();
}
