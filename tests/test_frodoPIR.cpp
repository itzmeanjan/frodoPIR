#include "frodoPIR/client.hpp"
#include "frodoPIR/internals/matrix/matrix.hpp"
#include "frodoPIR/internals/rng/prng.hpp"
#include "frodoPIR/server.hpp"
#include "gtest/gtest.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

template<size_t db_entry_count, size_t db_entry_byte_len, size_t mat_element_bitlen, size_t lwe_dimension>
static void
test_private_information_retrieval(const size_t num_queries)
{
  constexpr size_t λ = 128;
  constexpr size_t parsed_db_column_count = frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen);
  constexpr size_t db_byte_len = db_entry_count * db_entry_byte_len;
  constexpr size_t pub_matM_byte_len = frodoPIR_matrix::matrix_t<lwe_dimension, parsed_db_column_count>::get_byte_len();
  constexpr size_t query_byte_len = frodoPIR_vector::row_vector_t<db_entry_count>::get_byte_len();
  constexpr size_t response_byte_len = frodoPIR_vector::row_vector_t<parsed_db_column_count>::get_byte_len();

  std::array<uint8_t, λ / std::numeric_limits<uint8_t>::digits> seed_μ{};
  std::vector<uint8_t> db_bytes(db_byte_len, 0);
  std::vector<uint8_t> pub_matM_bytes(pub_matM_byte_len, 0);
  std::vector<uint8_t> query_bytes(query_byte_len, 0);
  std::vector<uint8_t> response_bytes(response_byte_len, 0);
  std::vector<uint8_t> db_row_bytes(db_entry_byte_len, 0);

  auto db_bytes_span = std::span<const uint8_t, db_byte_len>(db_bytes);
  auto pub_matM_bytes_span = std::span<uint8_t, pub_matM_byte_len>(pub_matM_bytes);
  auto query_bytes_span = std::span<uint8_t, query_byte_len>(query_bytes);
  auto response_bytes_span = std::span<uint8_t, response_byte_len>(response_bytes);
  auto db_row_bytes_span = std::span<uint8_t, db_entry_byte_len>(db_row_bytes);

  prng::prng_t prng{};

  prng.read(seed_μ);
  prng.read(db_bytes);

  auto [server, M] = frodoPIR_server::server_t<λ, db_entry_count, db_entry_byte_len, mat_element_bitlen, lwe_dimension>::setup(seed_μ, db_bytes_span);

  M.to_le_bytes(pub_matM_bytes_span);
  auto client = frodoPIR_client::client_t<λ, db_entry_count, db_entry_byte_len, mat_element_bitlen, lwe_dimension>::setup(seed_μ, pub_matM_bytes_span);

  for (size_t cur_query_count = 0; cur_query_count < num_queries; cur_query_count++) {
    const size_t db_row_index = [&]() {
      size_t buffer = 0;
      auto buffer_span = std::span<uint8_t, sizeof(buffer)>(reinterpret_cast<uint8_t*>(&buffer), sizeof(buffer));

      prng.read(buffer_span);

      return buffer % db_entry_count;
    }();

    client.prepare_query(db_row_index, prng);

    const auto is_query_ready = client.query(db_row_index, query_bytes_span);
    EXPECT_TRUE(is_query_ready);

    server.respond(query_bytes_span, response_bytes_span);

    const auto is_response_decoded = client.process_response(db_row_index, response_bytes_span, db_row_bytes_span);
    EXPECT_TRUE(is_response_decoded);

    const size_t db_row_begin_at = db_row_index * db_entry_byte_len;
    EXPECT_TRUE(std::ranges::equal(db_row_bytes_span, db_bytes_span.subspan(db_row_begin_at, db_entry_byte_len)));
  }
}

TEST(FrodoPIR, PrivateInformationRetrieval)
{
  test_private_information_retrieval<1ul << 16, 32, 10, 1774>(32);
  test_private_information_retrieval<1ul << 20, 32, 9, 1774>(32);
}
