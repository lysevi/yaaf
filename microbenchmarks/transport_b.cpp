#include <libnmq/local/l_transport.h>
#include <libnmq/network/net_transport.h>
#include <benchmark/benchmark.h>

#include <type_traits>

using namespace nmq::utils::logging;

namespace inner {

template <class T> using networkTransport = nmq::network::transport<T, size_t>;
template <class T> using localTransport = nmq::local::transport<T, size_t>;

template <typename Tr> struct ParamFiller {

  template <class Q = Tr>
  static typename std::enable_if<
      std::is_same<typename Q::params,
                   typename networkTransport<typename Q::arg_t>::params>::value,
      bool>::type
  fillParams(typename Q::params &t) {
    t.host = "localhost";
    t.port = 4040;
    return true;
  }

  template <class Q = Tr>
  static typename std::enable_if<
      std::is_same<typename Q::params,
                   typename localTransport<typename Q::arg_t>::params>::value,
      bool>::type
  fillParams(typename Q::params &t) {
    UNUSED(t);
    return true;
  }
};

} // namespace inner

template <class MockTrasport>
struct MockTransportListener : public MockTrasport::listener {
  MockTransportListener(std::shared_ptr<typename MockTrasport::manager> &manager,
                        typename MockTrasport::params &p)
      : MockTrasport::listener(manager, p) {
    _count.store(0);
  }

  void on_error(const typename MockTrasport::io_chanel_t::sender &,
               const nmq::ecode &er) override {
    UNUSED(er);
  };
  void on_message(const typename MockTrasport::io_chanel_t::sender &s,
                 typename const MockTrasport::arg_t &&d) override {
    logger("<= d", d);
    _count++;

    if (this->is_stoped()) {
      return;
    }

    this->send_async(s.id, _count.load());
  }

  std::atomic_size_t _count;
};

template <class MockTrasport>
struct MockTransportClient : public MockTrasport::connection {
  MockTransportClient(std::shared_ptr<typename MockTrasport::manager> &manager,
                      typename MockTrasport::params &p)
      : to_send(), MockTrasport::connection(manager, p) {}

  void send_query() { this->send_async(to_send); }

  void on_error(const nmq::ecode &er) override { UNUSED(er); };
  void on_message(const typename MockTrasport::result_t &&d) override {
    UNUSED(d);
    send_query();
  }

  typename MockTrasport::arg_t to_send;
};

template <class Tr> struct TransportTester : public benchmark::Fixture {
  std::shared_ptr<typename Tr::manager> manager;
  std::shared_ptr<MockTransportListener<Tr>> listener;
  std::shared_ptr<MockTransportClient<Tr>> client;

  void SetUp(const ::benchmark::State &) override {
    Tr::params p;

    inner::ParamFiller<Tr>::fillParams(p);

    manager = std::make_shared<typename Tr::manager>(p);

    manager->start();
    manager->wait_starting();

    listener = std::make_shared<MockTransportListener<Tr>>(manager, p);

    listener->start();
    listener->wait_starting();

    client = std::make_shared<MockTransportClient<Tr>>(manager, p);
    client->start();
    client->wait_starting();
    for (; !client->is_started();) {
      std::this_thread::yield();
    }
    client->send_query();
  }

  void TearDown(const ::benchmark::State &) override {
    client->stop();
    client->wait_stoping();

    listener->stop();
    listener->wait_stoping();

    manager->stop();
  }
};

BENCHMARK_TEMPLATE_F(TransportTester, NetUint8, inner::networkTransport<uint8_t>)
(benchmark::State &st) {
  while (st.KeepRunning()) {
  }
  st.counters["messages"] = (double)listener->_count.load();
}

BENCHMARK_TEMPLATE_F(TransportTester, LockfreeUint64, inner::localTransport<uint8_t>)
(benchmark::State &st) {
  while (st.KeepRunning()) {
  }
  st.counters["messages"] = (double)listener->_count.load();
}
