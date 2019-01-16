#include "helpers.h"

#include <boost/asio.hpp>

#include <libnmq/client.h>
#include <libnmq/server.h>
#include <libnmq/utils/logger.h>
#include <catch.hpp>


#include <functional>
#include <string>
#include <thread>

using namespace std::placeholders;
using namespace boost::asio;

using namespace nmq;
using namespace nmq::utils;

namespace server_client_test {

struct MockServer : public nmq::Server {

  MockServer(bool canswer, boost::asio::io_service *service, network::Listener::Params &p)
      : nmq::Server(service, p) {
    _canswer = canswer;
  }

  bool onNewLogin(const network::ListenerClient_Ptr i,
                  const queries::Login &lg) override {
    if (false == _canswer) {
      is_login_failed = true;
      return false;
    }
    return nmq::Server::onNewLogin(i, lg);
  }

  bool is_login_failed = false;
  bool _canswer;
};

bool server_stop = false;
std::shared_ptr<MockServer> server = nullptr;
boost::asio::io_service *service;
void server_thread(bool canswer) {
  network::Listener::Params p;
  p.port = 4040;
  service = new boost::asio::io_service();
  server = std::make_shared<MockServer>(canswer, service, p);

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

void testForReConnection(bool canswer, const size_t clients_count) {
  network::Connection::Params p("empty", "localhost", 4040);

  server_stop = false;
  std::thread t(server_thread, canswer);
  while (server == nullptr || !server->is_started()) {
    logger("server.client.testForReconnection. !server->is_started serverIsNull? ",
           server == nullptr);
  }

  std::vector<std::shared_ptr<Client>> clients(clients_count);
  for (size_t i = 0; i < clients_count; i++) {
    p.login = "client_" + std::to_string(i);
    if (false == canswer) {
      p.auto_reconnection = false;
    }
    clients[i] = std::make_shared<Client>(service, p);
    clients[i]->connectAsync();
  }

  if (false == canswer) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    while (!server->is_login_failed) {
      logger("server.client.testForReconnection. !is_login_failed");
    }
    for (auto &c : clients) {
      EXPECT_FALSE(c->is_connected());
    }
  } else {
    for (auto &c : clients) {
      while (!c->is_connected()) {
        logger("server.client.testForReconnection. client not connected");
      }
    }

    while (true) {
      auto users = server->users();
      bool loginned = false;
      for (auto u : users) {
        if (u.login != "server" && u.login.substr(0, 6) != "client") {
          loginned = false;
          break;
        } else {
          loginned = true;
        }
      }
      if (loginned && users.size() > clients_count) {
        break;
      }
      logger("server.client.testForReconnection. not all clients was loggined");
    }

    for (auto &c : clients) {
      c->disconnect();
      while (c->is_connected()) {
        logger("server.client.testForReconnection. client is still connected");
      }
    }
  }

  server_stop = true;
  while (server != nullptr) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  t.join();
}
} // namespace server_client_test

TEST_CASE("server.client.1") {
  const size_t connections_count = 1;
  server_client_test::testForReConnection(true, connections_count);
}

TEST_CASE("server.client.10") {
  const size_t connections_count = 10;
  server_client_test::testForReConnection(true, connections_count);
}

TEST_CASE("server.client.1.FailedLogin") {
  const size_t connections_count = 1;
  server_client_test::testForReConnection(false, connections_count);
}
