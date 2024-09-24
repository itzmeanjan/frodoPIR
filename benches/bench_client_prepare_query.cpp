#include "bench_common.hpp"
#include "frodoPIR/client.hpp"
#include "frodoPIR/server.hpp"
#include <benchmark/benchmark.h>
#include <cassert>

template<size_t λ, size_t db_entry_count, size_t db_entry_byte_len, size_t mat_element_bitlen, size_t lwe_dimension>
static void
bench_client_prepare_query(benchmark::State& state)
{
  constexpr size_t db_byte_len = db_entry_count * db_entry_byte_len;
  constexpr size_t parsed_db_column_count = frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen);
  constexpr size_t pub_matM_byte_len = frodoPIR_matrix::matrix_t<lwe_dimension, parsed_db_column_count>::get_byte_len();

  std::array<uint8_t, λ / std::numeric_limits<uint8_t>::digits> seed_μ{};
  std::vector<uint8_t> db_bytes(db_byte_len, 0);
  std::vector<uint8_t> pub_matM_bytes(pub_matM_byte_len, 0);

  auto seed_μ_span = std::span(seed_μ);
  auto db_bytes_span = std::span<uint8_t, db_byte_len>(db_bytes);
  auto pub_matM_bytes_span = std::span<uint8_t, pub_matM_byte_len>(pub_matM_bytes);

  prng::prng_t prng{};

  prng.read(seed_μ_span);
  prng.read(db_bytes_span);

  auto [server, M] = frodoPIR_server::server_t<λ, db_entry_count, db_entry_byte_len, mat_element_bitlen, lwe_dimension>::setup(seed_μ_span, db_bytes_span);

  M.to_le_bytes(pub_matM_bytes_span);
  auto client = frodoPIR_client::client_t<λ, db_entry_count, db_entry_byte_len, mat_element_bitlen, lwe_dimension>::setup(seed_μ, pub_matM_bytes_span);

  size_t rand_db_row_index = [&]() {
    size_t buffer = 0;
    auto buffer_span = std::span<uint8_t, sizeof(buffer)>(reinterpret_cast<uint8_t*>(&buffer), sizeof(buffer));

    prng.read(buffer_span);

    return buffer % db_entry_count;
  }();

  for (auto _ : state) {
    client.prepare_query(rand_db_row_index, prng);

    benchmark::DoNotOptimize(client);
    benchmark::DoNotOptimize(rand_db_row_index);
    benchmark::DoNotOptimize(prng);
    benchmark::ClobberMemory();

    rand_db_row_index ^= (rand_db_row_index << 1) ^ 1ul;
    rand_db_row_index %= db_entry_count;
  }

  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(bench_client_prepare_query<128, 1ul << 16, 256, 10, 1774>)
  ->Name("frodoPIR/client_prepare_query/2^16/256B")
  ->ComputeStatistics("min", compute_min)
  ->ComputeStatistics("max", compute_max);

BENCHMARK(bench_client_prepare_query<128, 1ul << 17, 256, 10, 1774>)
  ->Name("frodoPIR/client_prepare_query/2^17/256B")
  ->ComputeStatistics("min", compute_min)
  ->ComputeStatistics("max", compute_max);

BENCHMARK(bench_client_prepare_query<128, 1ul << 18, 256, 10, 1774>)
  ->Name("frodoPIR/client_prepare_query/2^18/256B")
  ->ComputeStatistics("min", compute_min)
  ->ComputeStatistics("max", compute_max);

BENCHMARK(bench_client_prepare_query<128, 1ul << 19, 256, 9, 1774>)
  ->Name("frodoPIR/client_prepare_query/2^19/256B")
  ->ComputeStatistics("min", compute_min)
  ->ComputeStatistics("max", compute_max);

BENCHMARK(bench_client_prepare_query<128, 1ul << 20, 256, 9, 1774>)
  ->Name("frodoPIR/client_prepare_query/2^20/256B")
  ->ComputeStatistics("min", compute_min)
  ->ComputeStatistics("max", compute_max);
