#include "bench_common.hpp"
#include "pir_online_phase_fixture.hpp"
#include <format>

BENCHMARK_DEFINE_F(FrodoPIROnlinePhaseFixture, ClientPrepareQuery)(benchmark::State& state)
{
  size_t db_row_idx = generate_random_db_index();

  auto query_bytes_span = std::span<uint8_t, query_byte_len>(query_bytes);
  auto response_bytes_span = std::span<uint8_t, response_byte_len>(response_bytes);
  auto db_row_bytes_span = std::span<uint8_t, db_entry_byte_len>(db_row_bytes);

  bool is_query_preprocessed = true;
  for (auto _ : state) {
    benchmark::DoNotOptimize(is_query_preprocessed);
    benchmark::DoNotOptimize(client_handle);
    benchmark::DoNotOptimize(db_row_idx);
    benchmark::DoNotOptimize(&csprng);

    is_query_preprocessed &= client_handle.prepare_query(db_row_idx, csprng);

    benchmark::ClobberMemory();

    // Prepare for next iteration, don't time it.
    state.PauseTiming();

    assert(client_handle.query(db_row_idx, query_bytes_span));
    server_handle.respond(query_bytes_span, response_bytes_span);
    assert(client_handle.process_response(db_row_idx, response_bytes_span, db_row_bytes_span));

    db_row_idx ^= (db_row_idx << 1) ^ 1ul;
    db_row_idx %= db_entry_count;

    state.ResumeTiming();
  }

  assert(is_query_preprocessed);
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK_REGISTER_F(FrodoPIROnlinePhaseFixture, ClientPrepareQuery)
  ->Name(std::format("frodoPIR/client_prepare_query/{}/{}", format_number(db_entry_count), format_bytes(db_entry_byte_len)))
  ->ComputeStatistics("min", compute_min)
  ->ComputeStatistics("max", compute_max)
  ->MeasureProcessCPUTime()
  ->UseRealTime()
  ->Unit(benchmark::kMillisecond);
