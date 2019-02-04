#include "helpers.h"

#include <libnmq/local/l_transport.h>
#include <libnmq/network/net_transport.h>

#include <catch.hpp>

#include <algorithm>
#include <iterator>
#include <vector>

using namespace nmq;
using namespace nmq::utils;
using namespace nmq::utils::logging;

struct MockMessage {
  uint64_t id;
  size_t client_id;
  std::string msg;
};

struct MockResultMessage {
  uint64_t id;
  size_t client_id;
  size_t length;
  std::string msg;

  MockResultMessage() = default;
  MockResultMessage(const MockResultMessage &) = default;
};

namespace nmq {
namespace serialization {
template <> struct object_packer<MockMessage> {
  using BinaryRW = nmq::serialization::binary_io<uint64_t, size_t, std::string>;

  static size_t capacity(const MockMessage &t) {
    return BinaryRW::capacity(t.id, t.client_id, t.msg);
  }
  template <class Iterator> static void pack(Iterator it, const MockMessage t) {
    return BinaryRW::write(it, t.client_id, t.id, t.msg);
  }
  template <class Iterator> static MockMessage unpack(Iterator ii) {
    MockMessage t{};
    BinaryRW::read(ii, t.client_id, t.id, t.msg);
    return t;
  }
};

template <> struct object_packer<MockResultMessage> {
  using BinaryRW =
      nmq::serialization::binary_io<uint64_t, size_t, size_t, std::string>;

  static size_t capacity(const MockResultMessage &t) {
    return BinaryRW::capacity(t.id, t.client_id, t.length, t.msg);
  }
  template <class Iterator> static void pack(Iterator it, const MockResultMessage t) {
    return BinaryRW::write(it, t.id, t.client_id, t.length, t.msg);
  }
  template <class Iterator> static MockResultMessage unpack(Iterator ii) {
    MockResultMessage t{};
    BinaryRW::read(ii, t.id, t.client_id, t.length, t.msg);
    return t;
  }
};
} // namespace serialization
} // namespace nmq

namespace {
const size_t TestableQSize = 10;
using networkTransport = nmq::network::transport<MockMessage, MockResultMessage>;
using localTransport = nmq::local::transport<MockMessage, MockResultMessage>;

template <typename T>
std::enable_if_t<std::is_same_v<T, networkTransport::params_t>, void> fillparams(T &t) {
  t.host = "localhost";
  t.port = 4040;
}

template <typename T>
std::enable_if_t<std::is_same_v<T, localTransport::params_t>, void> fillparams(T &t) {
  t.result_queue_size = 10;
  t.result_queue_size = 10;
}
} // namespace

template <class TestType> struct TransportTester {
  static void run(size_t clientsCount, size_t listenersCount) {
    using MockTrasport = TestType;

    struct listener : public MockTrasport::listener {
      listener(std::shared_ptr<MockTrasport::manager> &manager, MockTrasport::params_t &p)
          : MockTrasport::listener(manager, p) {}

      void on_error(const MockTrasport::io_chanel_t::sender_t &,
                   const ecode &er) override {

        if (er.inner_error == nmq::errors_kinds::FULL_STOP) {
          full_stop_flag = true;
        }
      };

      void on_message(const MockTrasport::io_chanel_t::sender_t &s,
                     const MockMessage &&d) override {

        if (is_stopping_started()) {
          return;
        }

        if (std::is_same_v<MockTrasport, localTransport>) {
          EXPECT_FALSE(is_bysy_test_flag);
        }

        is_bysy_test_flag = true;
        logger_info("<=id:", d.id, " msg:", d.msg);
        _locker.lock();
        _q.insert(std::make_pair(d.id, d.msg));
        _locker.unlock();

        MockResultMessage answer;
        answer.id = d.id;
        answer.client_id = d.client_id;
        answer.msg = d.msg + " " + d.msg;
        answer.length = answer.msg.size();

        if (this->is_stoped()) {
          return;
        }

        auto aor = this->send_async(s.id, answer);
        aor.wait();
        is_bysy_test_flag = false;
      }

      std::mutex _locker;
      std::map<uint64_t, std::string> _q;
      bool full_stop_flag = false;

      bool is_bysy_test_flag = false;
    };

    struct Client : public MockTrasport::connection {
      Client(std::shared_ptr<MockTrasport::manager> &manager,
             const MockTrasport::params_t &p)
          : MockTrasport::connection(manager, p) {}

      void on_connected() override { MockTrasport::connection::on_connected(); }

      nmq::async_operation_handler send_query() {
        MockMessage m;
        m.id = msg_id++;
        m.client_id = get_id().value;
        m.msg = "msg_" + std::to_string(m.id);
        logger_info("=>id:", m.id, " msg:", m.msg);
        auto aor = this->send_async(m);
        return aor;
      }

      void on_error(const ecode &er) override {
        if (er.inner_error == nmq::errors_kinds::ALL_LISTENERS_STOPED) {
          all_listeners__stoped_flag = true;
        }
        if (er.inner_error == nmq::errors_kinds::FULL_STOP) {
          full_stop_flag = true;
        }
        MockTrasport::connection::on_error(er);
      };

      void on_message(const MockResultMessage &&d) override {
        if (!is_stopping_started()) {
          logger_info("<=id:", d.id, " length:", d.length);
          _locker.lock();
          _q.insert(std::make_pair(d.id, d.length));
          _locker.unlock();
          ENSURE(d.client_id == get_id().value);
        }
      }

      size_t qSize() const {
        std::lock_guard<std::mutex> lg(_locker);
        return _q.size();
      }

      uint64_t msg_id = 1;

      mutable std::mutex _locker;
      std::map<uint64_t, size_t> _q;
      bool full_stop_flag = false;
      bool all_listeners__stoped_flag = false;
    };

    MockTrasport::params_t p;

    fillparams(p);
    p.threads_count = (listenersCount + clientsCount) + 3;
    auto manager = std::make_shared<MockTrasport::manager>(p);

    manager->start();
    manager->wait_starting();

    std::vector<std::shared_ptr<listener>> listeners(listenersCount);
    for (size_t i = 0; i < listenersCount; ++i) {
      auto l = std::make_shared<listener>(manager, p);
      listeners[i] = l;
      l->start();

      while (!l->is_started()) {
        logger("transport: !listener->is_started_flag");
        std::this_thread::yield();
      }
    }

    std::vector<std::shared_ptr<Client>> clients(clientsCount);

    for (size_t i = 0; i < clientsCount; ++i) {
      auto newClient = std::make_shared<Client>(manager, p);
      clients[i] = newClient;

      newClient->start();
      newClient->wait_starting();
    }

    auto checkF = [](std::shared_ptr<Client> c) { return c->_q.size() > TestableQSize; };

    for (;;) {

      for (auto client : clients) {
        auto aor = client->send_query();
        aor.wait();
      }

      if (std::all_of(clients.begin(), clients.end(), checkF)) {
        break;
      }
    }

    if (clientsCount > 1) {
      auto end = (size_t)(clientsCount / 2);
      for (size_t i = 0; i < end; ++i) {
        clients[i]->stop();
        clients[i]->wait_stoping();
        clients[i] = nullptr;
      }
    }

    logger("listener->stop()");
    for (size_t i = 0; i < listenersCount; ++i) {
      listeners[i]->stop();
      logger("listener = nullptr;");
      listeners[i] = nullptr;
    }

    for (auto client : clients) {
      if (client == nullptr) {
        continue;
      }
      if (std::is_same_v<MockTrasport, localTransport>) {

        while (!client->all_listeners__stoped_flag) {
          logger("transport: client->full_stop_flag");
          std::this_thread::yield();
        }

      } else {
        while (!client->is_stoped()) {
          logger("transport: client->is_stoped");
          std::this_thread::yield();
        }
      }
    }

    manager->stop();
    manager->wait_stoping();
    logger("manager->stop();");
    if (std::is_same_v<MockTrasport, localTransport>) {
      logger("check full_stop_flag");
      for (auto client : clients) {
        if (client != nullptr) {
          EXPECT_TRUE(client->full_stop_flag);
        }
      }
    }
  }
};

TEMPLATE_TEST_CASE("transport.1", "", networkTransport, localTransport) {
  TransportTester<TestType>::run(size_t(1), size_t(1));
}

TEMPLATE_TEST_CASE("transport.2x1", "", networkTransport, localTransport) {
  TransportTester<TestType>::run(size_t(2), size_t(1));
}

TEMPLATE_TEST_CASE("transport.2x2", "", localTransport) {
  TransportTester<TestType>::run(size_t(2), size_t(2));
}

TEMPLATE_TEST_CASE("transport.3x3", "", localTransport) {
  TransportTester<TestType>::run(size_t(3), size_t(3));
}
