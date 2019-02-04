#include "helpers.h"

#include <boost/asio.hpp>

#include <libnmq/network/connection.h>
#include <libnmq/network/listener.h>
#include <libnmq/network/net_transport.h>
#include <libnmq/utils/logger.h>

#include <catch.hpp>

#include <functional>
#include <string>
#include <thread>

using namespace nmq;
using namespace nmq::utils::logging;
using namespace nmq::utils;

namespace listener_test {

struct Listener : public nmq::network::abstract_listener_consumer {
  bool on_new_connection(nmq::network::listener_client_ptr) override {
    connections.fetch_add(1);
    return true;
  }

  void on_network_error(nmq::network::listener_client_ptr i,
                      const network::message_ptr & /*d*/,
                      const boost::system::error_code & /*err*/) override {}

  void on_new_message(nmq::network::listener_client_ptr i, network::message_ptr && /*d*/,
                    bool & /*cancel*/) override {}

  void on_disconnect(const nmq::network::listener_client_ptr & /*i*/) override {
    connections.fetch_sub(1);
  }

  std::atomic_int16_t connections = 0;
};

struct Connection : public nmq::network::abstract_connection_consumer {
  void on_connect() override { mock_is_connected = true; };
  void on_new_message(nmq::network::message_ptr &&, bool &) override {}
  void on_network_error(const nmq::network::message_ptr &,
                      const boost::system::error_code &err) override {
    bool isError = err == boost::asio::error::operation_aborted ||
                   err == boost::asio::error::connection_reset ||
                   err == boost::asio::error::eof;
    if (isError && !is_stoped()) {
      auto msg = err.message();
      nmq::utils::logging::logger_fatal(msg);
      EXPECT_FALSE(true);
    }
  }

  bool mock_is_connected = false;
  bool connection_error = false;
};

bool server_stop = false;
std::shared_ptr<nmq::network::listener> server = nullptr;
std::shared_ptr<Listener> lstnr = nullptr;
boost::asio::io_service *service;

void server_thread() {
  network::listener::params p;
  p.port = 4040;
  service = new boost::asio::io_service();

  server = std::make_shared<nmq::network::listener>(service, p);
  lstnr = std::make_shared<Listener>();
  server->add_consumer(lstnr.get());

  server->start();
  while (!server_stop) {
    service->poll_one();
  }

  server->stop();
  service->stop();
  while (!service->stopped()) {
  }
  EXPECT_TRUE(service->stopped());
  delete service;
  server = nullptr;
}

void testForConnection(const size_t clients_count) {
  network::connection::params p("localhost", 4040);

  server_stop = false;
  std::thread t(server_thread);
  while (server == nullptr || !server->is_started()) {
    logger("listener.client.testForConnection. !server->is_started serverIsNull? ",
           server == nullptr);
  }

  std::vector<std::shared_ptr<network::connection>> clients(clients_count);
  std::vector<std::shared_ptr<Connection>> consumers(clients_count);
  for (size_t i = 0; i < clients_count; i++) {
    clients[i] = std::make_shared<network::connection>(service, p);
    consumers[i] = std::make_shared<Connection>();
    clients[i]->add_consumer(consumers[i].get());
    clients[i]->start_async_connection();
  }

  for (auto &c : consumers) {
    while (!c->mock_is_connected) {
      logger("listener.client.testForConnection. client not connected");
    }
  }

  while (!lstnr->is_started() && lstnr->connections != clients_count) {
    logger("listener.client.testForConnection. not all clients was loggined");
  }

  for (auto &c : clients) {
    c->disconnect();
    while (!c->is_stoped()) {
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
