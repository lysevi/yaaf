#include "helpers.h"

#include <libnmq/lockfree/transport.h>
#include <libnmq/network/transport.h>

#include <catch.hpp>

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

  MockResultMessage() = default;
  MockResultMessage(const MockResultMessage &) = default;
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
  t.result_queue_size = 2;
}
} // namespace

TEMPLATE_TEST_CASE("transport", "", networkTransport, lockfreeTransport) {

  using MockTrasport = TestType;

  struct MockTransportListener : public MockTrasport::Listener {
    MockTransportListener(std::shared_ptr<MockTrasport::Manager> &manager,
                          MockTrasport::Params &p)
        : MockTrasport::Listener(manager.get(), p) {}

    void onStartComplete() override { is_started_flag = true; }

    void onError(const MockTrasport::io_chanel_type::Sender &,
                 const MockTrasport::io_chanel_type::ErrorCode & /*err*/) override {
      is_started_flag = false;
    };
    void onMessage(const MockTrasport::io_chanel_type::Sender &s, const MockMessage d,
                   bool &) override {
      logger_info("<=id:", d.id, " msg:", d.msg);
      _locker.lock();
      _q.insert(std::make_pair(d.id, d.msg));
      _locker.unlock();

      MockResultMessage answer;
      answer.id = d.id;
      answer.msg = d.msg + " " + d.msg;
      answer.length = answer.msg.size();

      if (this->isStopingBegin()) {
        return;
      }

      this->sendAsync(s.id, answer);
    }

    bool is_started_flag = false;
    std::mutex _locker;
    std::map<uint64_t, std::string> _q;
  };

  struct MockTransportClient : public MockTrasport::Connection {
    MockTransportClient(std::shared_ptr<MockTrasport::Manager> &manager,
                        const MockTrasport::Params &p)
        : MockTrasport::Connection(manager.get(), p) {}

    void onConnected() override { is_started_flag = true; }

    void sendQuery() {
      MockMessage m;
      m.id = msg_id++;
      m.msg = "msg_" + std::to_string(m.id);
      logger_info("=>id:", m.id, " msg:", m.msg);
      this->sendAsync(m);
    }

    void onError(const MockTrasport::io_chanel_type::ErrorCode & /*er*/) override {
      is_started_flag = false;
    };
    void onMessage(const MockResultMessage d, bool &) override {
      logger_info("<=id:", d.id, " length:", d.length);
      _locker.lock();
      _q.insert(std::make_pair(d.id, d.length));
      _locker.unlock();
      sendQuery();
    }

    size_t qSize() const {
      std::lock_guard<std::mutex> lg(_locker);
      return _q.size();
    }

    uint64_t msg_id = 1;
    bool is_started_flag = false;

    mutable std::mutex _locker;
    std::map<uint64_t, size_t> _q;
  };

  MockTrasport::Params p;

  fillParams(p);

  auto manager = std::make_shared<MockTrasport::Manager>(p);

  manager->start();

  auto listener = std::make_shared<MockTransportListener>(manager, p);

  listener->start();

  while (!listener->is_started_flag) {
    logger("transport: !listener->is_started_flag");
    std::this_thread::yield();
  }

  auto client = std::make_shared<MockTransportClient>(manager, p);
  client->start();

  while (!client->is_started_flag) {
    logger("transport: !client->is_started_flag");
    std::this_thread::yield();
  }

  client->sendQuery();
  while (client->qSize() < 10) {
    logger("transport: client->_q.size() < 10 :", client->_q.size());
    std::this_thread::yield();
  }

  logger("listener->stop()");
  listener->stop();
  logger("listener = nullptr;");

  while (client->is_started_flag) {
    logger("transport: client->is_started_flag");
    std::this_thread::yield();
  }
  manager->stop();
  logger("manager->stop();");
  
}
