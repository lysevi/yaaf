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

namespace listener_test {

struct MockListener : public nmq::network::Listener {
  MockListener(boost::asio::io_service *service, network::Listener::Params &p)
      : nmq::network::Listener(service, p) {}

  void onStartComplete() override { is_start_complete = true; }

  bool onNewConnection(nmq::network::ListenerClient_Ptr) override {
    connections.fetch_add(1);
    return true;
  }

  void onNetworkError(nmq::network::ListenerClient_Ptr i,
                      const network::message_ptr & /*d*/,
                      const boost::system::error_code & /*err*/) override {}

  void onNewMessage(nmq::network::ListenerClient_Ptr i,
                    const network::message_ptr & /*d*/, bool & /*cancel*/) override {}

  void onDisconnect(const nmq::network::ListenerClient_Ptr & /*i*/) override {
    connections.fetch_sub(1);
  }

  bool is_start_complete = false;
  std::atomic_int16_t connections = 0;
};

struct MockConnection : public nmq::network::Connection {
  MockConnection(boost::asio::io_service *service, const Params &_parms)
      : nmq::network::Connection(service, _parms) {}

  void onConnect() override { mock_is_connected = true; };
  void onNewMessage(const nmq::network::message_ptr &, bool &) override {}
  void onNetworkError(const nmq::network::message_ptr &,
                      const boost::system::error_code &err) override {
    bool isError = err == boost::asio::error::operation_aborted ||
                   err == boost::asio::error::connection_reset ||
                   err == boost::asio::error::eof;
    if (isError && !isStoped) {
      auto msg = err.message();
      logger_fatal(msg);
      EXPECT_FALSE(true);
    }
  }

  bool mock_is_connected = false;
  bool connection_error = false;
};

bool server_stop = false;
std::shared_ptr<MockListener> server = nullptr;
boost::asio::io_service *service;

void server_thread() {
  network::Listener::Params p;
  p.port = 4040;
  service = new boost::asio::io_service();
  server = std::make_shared<MockListener>(service, p);

  server->start();
  while (!server_stop) {
    service->poll_one();
  }

  server->stop();
  service->stop();
  EXPECT_TRUE(service->stopped());
  delete service;
  server = nullptr;
}

void testForConnection(const size_t clients_count) {
  network::Connection::Params p("empty", "localhost", 4040);

  server_stop = false;
  std::thread t(server_thread);
  while (server == nullptr || !server->is_started()) {
    logger("listener.client.testForConnection. !server->is_started serverIsNull? ",
           server == nullptr);
  }

  std::vector<std::shared_ptr<MockConnection>> clients(clients_count);
  for (size_t i = 0; i < clients_count; i++) {
    p.login = "client_" + std::to_string(i);
    clients[i] = std::make_shared<MockConnection>(service, p);
    clients[i]->async_connect();
  }

  for (auto &c : clients) {
    while (!c->mock_is_connected) {
      logger("listener.client.testForConnection. client not connected");
    }
  }

  while (!server->is_start_complete && server->connections != clients_count) {
    logger("listener.client.testForConnection. not all clients was loggined");
  }

  for (auto &c : clients) {
    c->disconnect();
    while (c->is_connected()) {
      logger("listener.client.testForConnection. client is still connected");
    }
  }

  server_stop = true;
  while (server != nullptr) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  t.join();
}
} // namespace listener_test

TEST_CASE("listener.client.1") {
  const size_t connections_count = 1;
  listener_test::testForConnection(connections_count);
}

TEST_CASE("listener.client.10") {
  const size_t connections_count = 10;
  listener_test::testForConnection(connections_count);
}

struct MockMessage {
  uint64_t id;
  std::string msg;
};

namespace nmq {
namespace serialization {
template <> struct ObjectScheme<MockMessage> {
  using Scheme = nmq::serialization::Scheme<uint64_t, std::string>;

  static size_t capacity(const MockMessage &t) { return Scheme::capacity(t.id, t.msg); }
  template <class Iterator> static void pack(Iterator it, const MockMessage t) {
    return Scheme::write(it, t.id, t.msg);
  }
  template <class Iterator> static MockMessage unpack(Iterator ii) {
    MockMessage t{};
    Scheme::read(ii, t.id, t.msg);
    return t;
  }
};
} // namespace serialization
} // namespace nmq
using MockTrasport = nmq::network::transport<MockMessage>;

struct MockTransportListener : public MockTrasport::listener_type {
  MockTransportListener(MockTrasport::params &p)
      : MockTrasport::listener_type(p.service, p) {}

  void onStartComplete() override { is_started_flag = true; }

  void onError(const MockTrasport::io_chanel_type::sender_type &,
               const MockTrasport::io_chanel_type::error_description &er) override{};
  void onMessage(const MockTrasport::io_chanel_type::sender_type &s, const MockMessage &d,
                 bool &) override {
    _q.insert(std::make_pair(d.id, d.msg));
    MockMessage answer;
    answer.id = d.id;
    answer.msg = d.msg + " " + d.msg;
    this->send_async(s.id, answer);
  }

  /**
  result - true for accept, false for failed.
  */
  bool onClient(const MockTrasport::io_chanel_type::sender_type &) override {
    return true;
  }
  void onClientDisconnect(const MockTrasport::io_chanel_type::sender_type &) override {}

  bool is_started_flag = false;
  std::map<uint64_t, std::string> _q;
};

struct MockTransportClient : public MockTrasport::connection_type {
  MockTransportClient(const MockTrasport::params &p, const std::string &login)
      : MockTrasport::connection_type(p.service, login, p) {}

  void onConnected() override { is_started_flag = true; }

  void sendQuery() {
    MockMessage m;
    m.id = msg_id++;
    m.msg = "msg_" + std::to_string(m.id);
    this->send_async(m);
  }

  void onError(const MockTrasport::io_chanel_type::error_description &er) override {
    is_started_flag = false;
  };
  void onMessage(const MockMessage &d, bool &) override {
    _q.insert(std::make_pair(d.id, d.msg));
    sendQuery();
  }

  uint64_t msg_id = 1;
  bool is_started_flag = false;
  std::map<uint64_t, std::string> _q;
};

TEST_CASE("transport") {

  boost::asio::io_service transport_service;
  MockTrasport::params p;
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
