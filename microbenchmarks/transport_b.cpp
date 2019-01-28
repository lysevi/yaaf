#include <libnmq/lockfree/l_transport.h>
#include <libnmq/network/net_transport.h>
#include <benchmark/benchmark.h>

#include <type_traits>

namespace inner {

template <class T> using networkTransport = nmq::network::Transport<T, size_t>;
template <class T> using lockfreeTransport = nmq::lockfree::Transport<T, size_t>;

template <typename Tr> struct ParamFiller {

  template <class Q = Tr>
  static typename std::enable_if<
      std::is_same<typename Q::Params,
                   typename networkTransport<typename Q::ArgType>::Params>::value,
      bool>::type
  fillParams(typename Q::Params &t) {
    t.host = "localhost";
    t.port = 4040;
    return true;
  }

  template <class Q = Tr>
  static typename std::enable_if<
      std::is_same<typename Q::Params,
                   typename lockfreeTransport<typename Q::ArgType>::Params>::value,
      bool>::type
  fillParams(typename Q::Params &t) {
    UNUSED(t);
    return true;
  }
};

} // namespace inner

template <class MockTrasport>
struct MockTransportListener : public MockTrasport::Listener {
  MockTransportListener(std::shared_ptr<typename MockTrasport::Manager> &manager,
                        typename MockTrasport::Params &p)
      : MockTrasport::Listener(manager, p) {
    _count.store(0);
  }

  void onError(const typename MockTrasport::io_chanel_type::Sender &,
               const nmq::ErrorCode &er) override {
    UNUSED(er);
  };
  void onMessage(const typename MockTrasport::io_chanel_type::Sender &s,
                 typename const MockTrasport::ArgType d, bool &) override {
    nmq::logger("<= d", d);
    _count++;

    if (this->isStoped()) {
      return;
    }

    this->sendAsync(s.id, _count.load());
  }

  std::atomic_size_t _count;
};

template <class MockTrasport>
struct MockTransportClient : public MockTrasport::Connection {
  MockTransportClient(std::shared_ptr<typename MockTrasport::Manager> &manager,
                      typename MockTrasport::Params &p)
      : toSend(), MockTrasport::Connection(manager, p) {}

  void sendQuery() { this->sendAsync(toSend); }

  void onError(const nmq::ErrorCode &er) override { UNUSED(er); };
  void onMessage(const typename MockTrasport::ResultType d, bool &) override {
    UNUSED(d);
    sendQuery();
  }

  typename MockTrasport::ArgType toSend;
};

template <class Tr> struct TransportTester : public benchmark::Fixture {
  std::shared_ptr<typename Tr::Manager> manager;
  std::shared_ptr<MockTransportListener<Tr>> listener;
  std::shared_ptr<MockTransportClient<Tr>> client;

  void SetUp(const ::benchmark::State &) override {
    Tr::Params p;

    inner::ParamFiller<Tr>::fillParams(p);

    manager = std::make_shared<typename Tr::Manager>(p);

    manager->start();

    listener = std::make_shared<MockTransportListener<Tr>>(manager, p);

    listener->start();

    while (!listener->isStarted()) {
    }

    client = std::make_shared<MockTransportClient<Tr>>(manager, p);
    client->start();
    for (; !client->isStarted();) {
      std::this_thread::yield();
    }
    client->sendQuery();
  }

  void TearDown(const ::benchmark::State &) override {
    client->stop();
    client->waitStoping();

    listener->stop();
    listener->waitStoping();

    manager->stop();
  }
};

BENCHMARK_TEMPLATE_F(TransportTester, NetUint8, inner::networkTransport<uint8_t>)
(benchmark::State &st) {
  while (st.KeepRunning()) {
  }
  st.counters["messages"] = (double)listener->_count.load();
}

BENCHMARK_TEMPLATE_F(TransportTester, LockfreeUint64, inner::lockfreeTransport<uint8_t>)
(benchmark::State &st) {
  while (st.KeepRunning()) {
  }
  st.counters["messages"] = (double)listener->_count.load();
}
