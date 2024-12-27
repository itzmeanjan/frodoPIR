#include "bench_common.hpp"
#include "frodoPIR/server.hpp"
#include <benchmark/benchmark.h>
#include <format>

static constexpr size_t db_entry_count = 1ul << 20;
static constexpr size_t db_entry_byte_len = 1024;
static constexpr size_t mat_element_bitlen = 9;

static void
bench_server_setup(benchmark::State& state)
{
  using server_t = frodoPIR_server::server_t<db_entry_count, db_entry_byte_len, mat_element_bitlen>;

  constexpr size_t db_byte_len = db_entry_count * db_entry_byte_len;

  std::array<uint8_t, frodoPIR_server::λ / std::numeric_limits<uint8_t>::digits> seed_μ{};
  std::vector<uint8_t> db_bytes(db_byte_len, 0);

  auto seed_μ_span = std::span(seed_μ);
  auto db_bytes_span = std::span<uint8_t, db_byte_len>(db_bytes);

  csprng::csprng_t csprng{};

  csprng.generate(seed_μ_span);
  csprng.generate(db_bytes_span);

  for (auto _ : state) {
    benchmark::DoNotOptimize(seed_μ_span);
    benchmark::DoNotOptimize(db_bytes_span);

    auto [server, M] = server_t::setup(seed_μ_span, db_bytes_span);

    benchmark::DoNotOptimize(server);
    benchmark::DoNotOptimize(M);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(bench_server_setup)
  ->Name(std::format("frodoPIR/server_setup/{}/{}", format_number(db_entry_count), format_bytes(db_entry_byte_len)))
  ->ComputeStatistics("min", compute_min)
  ->ComputeStatistics("max", compute_max)
  ->MeasureProcessCPUTime()
  ->UseRealTime()
  ->Unit(benchmark::kSecond);
