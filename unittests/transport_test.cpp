#include "helpers.h"

#include <libnmq/lockfree/transport.h>
#include <libnmq/network/transport.h>

#include <catch.hpp>

#include <iterator>
#include <vector>

using namespace nmq;
using namespace nmq::utils;

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
template <> struct ObjectScheme<MockMessage> {
  using BinaryRW = nmq::serialization::BinaryReaderWriter<uint64_t, size_t, std::string>;

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

template <> struct ObjectScheme<MockResultMessage> {
  using BinaryRW =
      nmq::serialization::BinaryReaderWriter<uint64_t, size_t, size_t, std::string>;

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
using networkTransport = nmq::network::Transport<MockMessage, MockResultMessage>;
using lockfreeTransport = nmq::lockfree::Transport<MockMessage, MockResultMessage>;

template <typename T>
std::enable_if_t<std::is_same_v<T, networkTransport::Params>, void> fillParams(T &t) {
  t.host = "localhost";
  t.port = 4040;
}

template <typename T>
std::enable_if_t<std::is_same_v<T, lockfreeTransport::Params>, void> fillParams(T &t) {
  t.result_queue_size = 10;
  t.result_queue_size = 10;
}
} // namespace

template <class TestType> struct TransportTester {
  static void run(size_t clientsCount, size_t listenersCount) {
    using MockTrasport = TestType;

    struct MockTransportListener : public MockTrasport::Listener {
      MockTransportListener(std::shared_ptr<MockTrasport::Manager> &manager,
                            MockTrasport::Params &p)
          : MockTrasport::Listener(manager, p) {}

      void onError(const MockTrasport::io_chanel_type::Sender &,
                   const ErrorCode &er) override {

        if (er.inner_error == nmq::ErrorsKinds::FULL_STOP) {
          full_stop_flag = true;
        }
      };
      void onMessage(const MockTrasport::io_chanel_type::Sender &s, const MockMessage d,
                     bool &) override {
        if (isStopBegin()) {
          return;
        }
        logger_info("<=id:", d.id, " msg:", d.msg);
        _locker.lock();
        _q.insert(std::make_pair(d.id, d.msg));
        _locker.unlock();

        MockResultMessage answer;
        answer.id = d.id;
        answer.client_id = d.client_id;
        answer.msg = d.msg + " " + d.msg;
        answer.length = answer.msg.size();

        if (this->isStoped()) {
          return;
        }

        this->sendAsync(s.id, answer);
      }

      std::mutex _locker;
      std::map<uint64_t, std::string> _q;
      bool full_stop_flag = false;
    };

    struct MockTransportClient : public MockTrasport::Connection {
      MockTransportClient(std::shared_ptr<MockTrasport::Manager> &manager,
                          const MockTrasport::Params &p, size_t id_)
          : MockTrasport::Connection(manager, p), id(id_) {}

      void onConnected() override { MockTrasport::Connection::onConnected(); }

      void sendQuery() {
        MockMessage m;
        m.id = msg_id++;
        m.client_id = id;
        m.msg = "msg_" + std::to_string(m.id);
        logger_info("=>id:", m.id, " msg:", m.msg);
        this->sendAsync(m);
      }

      void onError(const ErrorCode &er) override {
        if (er.inner_error == nmq::ErrorsKinds::ALL_LISTENERS_STOPED) {
          all_listeners__stoped_flag = true;
        }
        if (er.inner_error == nmq::ErrorsKinds::FULL_STOP) {
          full_stop_flag = true;
        }
        MockTrasport::Connection::onError(er);
      };
      void onMessage(const MockResultMessage d, bool &) override {
        logger_info("<=id:", d.id, " length:", d.length);
        _locker.lock();
        _q.insert(std::make_pair(d.id, d.length));
        _locker.unlock();
        EXPECT_EQ(d.client_id, d.client_id);
        sendQuery();
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

      size_t id;
    };

    MockTrasport::Params p;

    fillParams(p);
    p.threads_count = (listenersCount + clientsCount) * 2;
    auto manager = std::make_shared<MockTrasport::Manager>(p);

    manager->start();
    manager->waitStarting();

    std::vector<std::shared_ptr<MockTransportListener>> listeners(listenersCount);
    for (size_t i = 0; i < listenersCount; ++i) {
      auto listener = std::make_shared<MockTransportListener>(manager, p);
      listeners[i] = listener;
      listener->start();

      while (!listener->isStarted()) {
        logger("transport: !listener->is_started_flag");
        std::this_thread::yield();
      }
    }

    std::vector<std::shared_ptr<MockTransportClient>> clients(clientsCount);

    for (size_t i = 0; i < clientsCount; ++i) {
      auto newClient = std::make_shared<MockTransportClient>(manager, p, i + 1);
      clients[i] = newClient;

      newClient->start();

      while (!newClient->isStarted()) {
        logger("transport: !newClient->is_started_flag");
        std::this_thread::yield();
      }

      newClient->sendQuery();
    }

    for (auto client : clients) {
      client->sendQuery();
    }

    for (auto client : clients) {
      while (client->qSize() < 10) {
        logger("transport: client->_q.size() < 10 :", client->_q.size());
        std::this_thread::yield();
      }
    }

    if (clientsCount > 1) {
      auto end = (size_t)clientsCount % 2;
      for (size_t i = 0; i < end; ++i) {
        clients[i]->stop();
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
      if (std::is_same_v<MockTrasport, lockfreeTransport>) {

        while (!client->all_listeners__stoped_flag) {
          logger("transport: client->full_stop_flag");
          std::this_thread::yield();
        }

      } else {
        while (!client->isStoped()) {
          logger("transport: client->isStoped");
          std::this_thread::yield();
        }
      }
    }

    manager->stop();
    manager->waitStoping();
    logger("manager->stop();");
    if (std::is_same_v<MockTrasport, lockfreeTransport>) {
      logger("check full_stop_flag");
      for (auto client : clients) {
        if (client != nullptr) {
          EXPECT_TRUE(client->full_stop_flag);
        }
      }
    }
  }
};

TEMPLATE_TEST_CASE("transport.1", "", networkTransport, lockfreeTransport) {
  TransportTester<TestType>::run(size_t(1), size_t(1));
}

TEMPLATE_TEST_CASE("transport.2x1", "", networkTransport, lockfreeTransport) {
  TransportTester<TestType>::run(size_t(2), size_t(1));
}

//TEMPLATE_TEST_CASE("transport.10x2", "", networkTransport, lockfreeTransport) {
//  TransportTester<TestType>::run(size_t(10), size_t(2));
//}
//
//TEMPLATE_TEST_CASE("transport.10x1", "", lockfreeTransport) {
//  TransportTester<TestType>::run(size_t(10), size_t(1));
//}
