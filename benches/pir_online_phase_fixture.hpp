#pragma once
#include "frodoPIR/client.hpp"
#include "frodoPIR/server.hpp"
#include <benchmark/benchmark.h>
#include <cassert>
#include <vector>

static constexpr size_t db_entry_count = 1ul << 20;
static constexpr size_t db_entry_byte_len = 1024;
static constexpr size_t mat_element_bitlen = 9;

static constexpr size_t db_byte_len = db_entry_count * db_entry_byte_len;
static constexpr size_t parsed_db_column_count = frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen);
static constexpr size_t pub_matM_byte_len = frodoPIR_matrix::matrix_t<frodoPIR_server::LWE_DIMENSION, parsed_db_column_count>::get_byte_len();
static constexpr size_t query_byte_len = frodoPIR_vector::row_vector_t<db_entry_count>::get_byte_len();
static constexpr size_t response_byte_len = frodoPIR_vector::row_vector_t<parsed_db_column_count>::get_byte_len();

class FrodoPIROnlinePhaseFixture : public benchmark::Fixture
{
public:
  using server_t = frodoPIR_server::server_t<db_entry_count, db_entry_byte_len, mat_element_bitlen>;
  using client_t = frodoPIR_client::client_t<db_entry_count, db_entry_byte_len, mat_element_bitlen>;

  std::array<uint8_t, frodoPIR_server::λ / std::numeric_limits<uint8_t>::digits> seed_μ{};
  std::vector<uint8_t> db_bytes;
  std::vector<uint8_t> pub_matM_bytes;
  std::vector<uint8_t> query_bytes;
  std::vector<uint8_t> response_bytes;
  std::vector<uint8_t> db_row_bytes;

  csprng::csprng_t csprng{};

  server_t server_handle;
  client_t client_handle;

  size_t generate_random_db_index()
  {
    size_t buffer = 0;
    auto buffer_span = std::span<uint8_t, sizeof(buffer)>(reinterpret_cast<uint8_t*>(&buffer), sizeof(buffer));

    csprng.generate(buffer_span);
    return buffer % db_entry_count;
  }

  void SetUp(benchmark::State& state) override
  {
    if (state.thread_index() == 0) {
      db_bytes = std::vector<uint8_t>(db_byte_len, 0);
      pub_matM_bytes = std::vector<uint8_t>(pub_matM_byte_len, 0);
      query_bytes = std::vector<uint8_t>(query_byte_len, 0);
      response_bytes = std::vector<uint8_t>(response_byte_len, 0);
      db_row_bytes = std::vector<uint8_t>(db_entry_byte_len, 0);

      auto seed_μ_span = std::span(seed_μ);
      auto db_bytes_span = std::span<uint8_t, db_byte_len>(db_bytes);
      auto pub_matM_bytes_span = std::span<uint8_t, pub_matM_byte_len>(pub_matM_bytes);

      csprng.generate(seed_μ_span);
      csprng.generate(db_bytes_span);

      auto [server, M] = server_t::setup(seed_μ_span, db_bytes_span);
      server_handle = server;

      M.to_le_bytes(pub_matM_bytes_span);

      auto client = client_t::setup(seed_μ, pub_matM_bytes_span);
      client_handle = client;
    }
  }

  void TearDown(benchmark::State& state) override
  {
    if (state.thread_index() == 0) {
      std::ranges::fill(seed_μ, 0);
      std::ranges::fill(db_bytes, 0);
      std::ranges::fill(pub_matM_bytes, 0);
      std::ranges::fill(query_bytes, 0);
      std::ranges::fill(response_bytes, 0);
      std::ranges::fill(db_row_bytes, 0);

      db_bytes.resize(0);
      pub_matM_bytes.resize(0);
      query_bytes.resize(0);
      response_bytes.resize(0);
      db_row_bytes.resize(0);
    }
  }
};
