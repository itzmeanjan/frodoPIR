#include "bench_common.hpp"
#include "frodoPIR/server.hpp"
#include <benchmark/benchmark.h>

template<size_t λ, size_t db_entry_count, size_t db_entry_byte_len, size_t mat_element_bitlen, size_t lwe_dimension>
static void
bench_server_setup(benchmark::State& state)
{
  using server_t = frodoPIR_server::server_t<λ, db_entry_count, db_entry_byte_len, mat_element_bitlen, lwe_dimension>;

  constexpr size_t db_byte_len = db_entry_count * db_entry_byte_len;

  std::array<uint8_t, λ / std::numeric_limits<uint8_t>::digits> seed_μ{};
  std::vector<uint8_t> db_bytes(db_byte_len, 0);

  auto seed_μ_span = std::span(seed_μ);
  auto db_bytes_span = std::span<uint8_t, db_byte_len>(db_bytes);

  csprng::csprng_t csprng{};

  csprng.generate(seed_μ_span);
  csprng.generate(db_bytes_span);

  for (auto _ : state) {
    auto [server, M] = server_t::setup(seed_μ_span, db_bytes_span);

    benchmark::DoNotOptimize(seed_μ_span);
    benchmark::DoNotOptimize(db_bytes_span);
    benchmark::DoNotOptimize(server);
    benchmark::DoNotOptimize(M);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(bench_server_setup<128, 1ul << 16, 256, 10, 1774>)
  ->Name("frodoPIR/server_setup/2^16/256B")
  ->ComputeStatistics("min", compute_min)
  ->ComputeStatistics("max", compute_max)
  ->MeasureProcessCPUTime()
  ->UseRealTime()
  ->Unit(benchmark::kSecond);

BENCHMARK(bench_server_setup<128, 1ul << 17, 256, 10, 1774>)
  ->Name("frodoPIR/server_setup/2^17/256B")
  ->ComputeStatistics("min", compute_min)
  ->ComputeStatistics("max", compute_max)
  ->MeasureProcessCPUTime()
  ->UseRealTime()
  ->Unit(benchmark::kSecond);

BENCHMARK(bench_server_setup<128, 1ul << 18, 256, 10, 1774>)
  ->Name("frodoPIR/server_setup/2^18/256B")
  ->ComputeStatistics("min", compute_min)
  ->ComputeStatistics("max", compute_max)
  ->MeasureProcessCPUTime()
  ->UseRealTime()
  ->Unit(benchmark::kSecond);

BENCHMARK(bench_server_setup<128, 1ul << 19, 256, 9, 1774>)
  ->Name("frodoPIR/server_setup/2^19/256B")
  ->ComputeStatistics("min", compute_min)
  ->ComputeStatistics("max", compute_max)
  ->MeasureProcessCPUTime()
  ->UseRealTime()
  ->Unit(benchmark::kSecond);

BENCHMARK(bench_server_setup<128, 1ul << 20, 256, 9, 1774>)
  ->Name("frodoPIR/server_setup/2^20/256B")
  ->ComputeStatistics("min", compute_min)
  ->ComputeStatistics("max", compute_max)
  ->MeasureProcessCPUTime()
  ->UseRealTime()
  ->Unit(benchmark::kSecond);
