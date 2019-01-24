#include <libnmq/network/queries.h>
#include <libnmq/serialization/serialization.h>
#include <benchmark/benchmark.h>

#include <array>

using namespace nmq;
using namespace nmq::network::queries;
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
  std::array<uint8_t, 1024 * 10> buffer;
  std::fill(buffer.begin(), buffer.end(), uint8_t(0));

  auto it = buffer.data();
  auto cap = nmq::serialization::BinaryReaderWriter<T>::capacity(arg);
  for (auto _ : state) {
    nmq::serialization::BinaryReaderWriter<T>::write(it, arg);
    if (it == (buffer.data() + buffer.size() - cap)) {
      it = buffer.data();
    }
  }
}
BENCHMARK_CAPTURE(BM_serialisation_pack, uint8_t, uint8_t(10));
BENCHMARK_CAPTURE(BM_serialisation_pack, uint16_t, uint16_t(10));
BENCHMARK_CAPTURE(BM_serialisation_pack, uint32_t, uint32_t(10));
BENCHMARK_CAPTURE(BM_serialisation_pack, uint64_t, uint64_t(10));
BENCHMARK_CAPTURE(BM_serialisation_pack, std::string_small, small_string);
BENCHMARK_CAPTURE(BM_serialisation_pack, std::string_medium, medium_string);

template <class T> void BM_serialisation_unpack(benchmark::State &state, T arg) {
  auto cap = nmq::serialization::BinaryReaderWriter<T>::capacity(arg);

  std::vector<uint8_t> buffer(cap);
  std::fill(buffer.begin(), buffer.end(), uint8_t(0));
  
  auto it = buffer.data();
  nmq::serialization::BinaryReaderWriter<T>::write(it, arg);

  for (auto _ : state) {
    it = buffer.data();
    T result;
    nmq::serialization::BinaryReaderWriter<T>::read(it, result);
  }
}

BENCHMARK_CAPTURE(BM_serialisation_unpack, uint8_t, uint8_t(10));
BENCHMARK_CAPTURE(BM_serialisation_unpack, uint16_t, uint16_t(10));
BENCHMARK_CAPTURE(BM_serialisation_unpack, uint32_t, uint32_t(10));
BENCHMARK_CAPTURE(BM_serialisation_unpack, uint64_t, uint64_t(10));
BENCHMARK_CAPTURE(BM_serialisation_unpack, std::string_small, small_string);
BENCHMARK_CAPTURE(BM_serialisation_unpack, std::string_medium, medium_string);

BENCHMARK_DEFINE_F(Serialisation, Ok)(benchmark::State &state) {
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(Ok(uint64_t(1)).getMessage());
  }
}
BENCHMARK_REGISTER_F(Serialisation, Ok);

BENCHMARK_DEFINE_F(Serialisation, Login)(benchmark::State &state) {
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(Login("login").getMessage());
  }
}
BENCHMARK_REGISTER_F(Serialisation, Login);

BENCHMARK_DEFINE_F(Serialisation, LoginConfirm)(benchmark::State &state) {
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(LoginConfirm(uint64_t(1)).getMessage());
  }
}
BENCHMARK_REGISTER_F(Serialisation, LoginConfirm);
