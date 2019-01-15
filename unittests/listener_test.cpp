#include "helpers.h"
#include <libnmq/network/connection.h>
#include <libnmq/network/listener.h>
#include <libnmq/utils/logger.h>
#include <catch.hpp>

#include <boost/asio.hpp>

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
      : nmq::network::Listener(service, p) {
  }

  void onStartComplete() override { is_start_complete = true; }

  bool 
  onNewConnection(ClientConnection_Ptr i) override {
    connections.fetch_add(1);
    return true;
  }

  void onNetworkError(ClientConnection_Ptr i, const network::Message_ptr &d,
                      const boost::system::error_code &err) override {}

  void onNewMessage(ClientConnection_Ptr i, const network::Message_ptr &d,
                    bool &cancel) override {}

  void onDisconnect(const Listener::ClientConnection_Ptr &i) override {
    connections.fetch_sub(1);
  }

  bool is_start_complete = false;
  std::atomic_int16_t connections = 0;
};

struct MockConnection : public nmq::network::Connection {
  MockConnection(boost::asio::io_service *service, const Params &_parms)
      : nmq::network::Connection(service, _parms) {}

  void onConnect() override { mock_is_connected = true; };
  void onNewMessage(const nmq::network::Message_ptr &d, bool &cancel) override {}
  void onNetworkError(const nmq::network::Message_ptr &d,
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

