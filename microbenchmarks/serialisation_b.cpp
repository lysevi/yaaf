#include <libyaaf/network/queries.h>
#include <libyaaf/serialization/serialization.h>
#include <benchmark/benchmark.h>

#include <array>

using namespace yaaf;
using namespace yaaf::network::queries;
namespace {
const std::string small_string = "small";
const std::string medium_string =
    "medium string medium string medium string medium string medium string";
} // namespace

class Serialisation : public benchmark::Fixture {
  virtual void SetUp(const ::benchmark::State &) {}

  virtual void TearDown(const ::benchmark::State &) {}

public:
};

template <class T> void BM_serialisation_pack(benchmark::State &state, T arg) {
  std::array<uint8_t, 1024> buffer;
  std::fill(buffer.begin(), buffer.end(), uint8_t(0));

  for (auto _ : state) {
    auto it = buffer.data();
    yaaf::serialization::binary_io<T>::write(it, arg);
  }
}
BENCHMARK_CAPTURE(BM_serialisation_pack, uint8_t, uint8_t(10));
BENCHMARK_CAPTURE(BM_serialisation_pack, uint16_t, uint16_t(10));
BENCHMARK_CAPTURE(BM_serialisation_pack, uint32_t, uint32_t(10));
BENCHMARK_CAPTURE(BM_serialisation_pack, uint64_t, uint64_t(10));
BENCHMARK_CAPTURE(BM_serialisation_pack, std::string_small, small_string);
BENCHMARK_CAPTURE(BM_serialisation_pack, std::string_medium, medium_string);

template <class T> void BM_serialisation_unpack(benchmark::State &state, T arg) {
  auto cap = yaaf::serialization::binary_io<T>::capacity(arg);

  std::vector<uint8_t> buffer(cap);
  std::fill(buffer.begin(), buffer.end(), uint8_t(0));

  auto it = buffer.data();
  yaaf::serialization::binary_io<T>::write(it, arg);

  for (auto _ : state) {
    it = buffer.data();
    T result;
    yaaf::serialization::binary_io<T>::read(it, result);
  }
}

BENCHMARK_CAPTURE(BM_serialisation_unpack, uint8_t, uint8_t(10));
BENCHMARK_CAPTURE(BM_serialisation_unpack, uint16_t, uint16_t(10));
BENCHMARK_CAPTURE(BM_serialisation_unpack, uint32_t, uint32_t(10));
BENCHMARK_CAPTURE(BM_serialisation_unpack, uint64_t, uint64_t(10));
BENCHMARK_CAPTURE(BM_serialisation_unpack, std::string_small, small_string);
BENCHMARK_CAPTURE(BM_serialisation_unpack, std::string_medium, medium_string);

BENCHMARK_DEFINE_F(Serialisation, ok)(benchmark::State &state) {
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(ok(uint64_t(1)).get_message());
  }
}
BENCHMARK_REGISTER_F(Serialisation, ok);

BENCHMARK_DEFINE_F(Serialisation, login)(benchmark::State &state) {
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(login("login").get_message());
  }
}
BENCHMARK_REGISTER_F(Serialisation, login);

BENCHMARK_DEFINE_F(Serialisation, login_confirm)(benchmark::State &state) {
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(login_confirm(uint64_t(1)).get_message());
  }
}
BENCHMARK_REGISTER_F(Serialisation, login_confirm);
