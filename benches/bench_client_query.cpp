#include "bench_common.hpp"
#include "pir_online_phase_fixture.hpp"
#include <format>

BENCHMARK_DEFINE_F(FrodoPIROnlinePhaseFixture, ClientQuery)(benchmark::State& state)
{
  size_t db_row_idx = generate_random_db_index();

  auto query_bytes_span = std::span<uint8_t, query_byte_len>(query_bytes);
  auto response_bytes_span = std::span<uint8_t, response_byte_len>(response_bytes);
  auto db_row_bytes_span = std::span<uint8_t, db_entry_byte_len>(db_row_bytes);

  bool is_query_ready = true;
  for (auto _ : state) {
    benchmark::DoNotOptimize(is_query_ready);
    benchmark::DoNotOptimize(client_handle);
    benchmark::DoNotOptimize(db_row_idx);
    benchmark::DoNotOptimize(query_bytes_span);

    is_query_ready &= client_handle.query(db_row_idx, query_bytes_span);

    benchmark::ClobberMemory();

    // Prepare for next iteration, don't time it.
    state.PauseTiming();

    server_handle.respond(query_bytes_span, response_bytes_span);
    assert(client_handle.process_response(db_row_idx, response_bytes_span, db_row_bytes_span));

    db_row_idx ^= (db_row_idx << 1) ^ 1ul;
    db_row_idx %= db_entry_count;

    assert(client_handle.prepare_query(db_row_idx, csprng));

    state.ResumeTiming();
  }

  assert(is_query_ready);
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK_REGISTER_F(FrodoPIROnlinePhaseFixture, ClientQuery)
  ->Name(std::format("frodoPIR/client_query/{}/{}", format_number(db_entry_count), format_bytes(db_entry_byte_len)))
  ->ComputeStatistics("min", compute_min)
  ->ComputeStatistics("max", compute_max)
  ->MeasureProcessCPUTime()
  ->UseRealTime()
  ->Unit(benchmark::kMicrosecond);
