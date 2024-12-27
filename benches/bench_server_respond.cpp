#include "bench_common.hpp"
#include "pir_online_phase_fixture.hpp"
#include <format>

BENCHMARK_DEFINE_F(FrodoPIROnlinePhaseFixture, ServerRespond)(benchmark::State& state)
{
  const size_t db_row_idx = generate_random_db_index();

  auto query_bytes_span = std::span<uint8_t, query_byte_len>(query_bytes);
  auto response_bytes_span = std::span<uint8_t, response_byte_len>(response_bytes);

  assert(client_handle.prepare_query(db_row_idx, csprng));
  assert(client_handle.query(db_row_idx, query_bytes_span));

  for (auto _ : state) {
    benchmark::DoNotOptimize(server_handle);
    benchmark::DoNotOptimize(query_bytes_span);
    benchmark::DoNotOptimize(response_bytes_span);

    server_handle.respond(query_bytes_span, response_bytes_span);

    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations());
}

BENCHMARK_REGISTER_F(FrodoPIROnlinePhaseFixture, ServerRespond)
  ->Name(std::format("frodoPIR/server_respond/{}", format_bytes(db_byte_len)))
  ->ComputeStatistics("min", compute_min)
  ->ComputeStatistics("max", compute_max)
  ->MeasureProcessCPUTime()
  ->UseRealTime()
  ->Unit(benchmark::kMillisecond);
