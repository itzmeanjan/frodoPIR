#include "frodoPIR/client.hpp"
#include "frodoPIR/server.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>

// Given a bytearray of length N, this function converts it to human readable hex string of length N << 1 | N >= 0
static inline const std::string
to_hex(std::span<const uint8_t> bytes)
{
  std::stringstream ss;
  ss << std::hex;

  for (size_t i = 0; i < bytes.size(); i++) {
    ss << std::setw(2) << std::setfill('0') << static_cast<uint32_t>(bytes[i]);
  }

  return ss.str();
}

// Compile with
// g++ -std=c++20 -Wall -Wextra -pedantic -O3 -march=native -I include -I sha3/include examples/frodoPIR.cpp
int
main()
{
  // Parameter setup for instantiating FrodoPIR
  constexpr size_t λ = 128;
  constexpr size_t db_entry_count = 1ul << 16;
  constexpr size_t db_entry_byte_len = 32;
  constexpr size_t mat_element_bitlen = 10;
  constexpr size_t lwe_dimension = 1774;

  // Database, query and response byte length
  constexpr size_t parsed_db_column_count = frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen);
  constexpr size_t db_byte_len = db_entry_count * db_entry_byte_len;
  constexpr size_t pub_matM_byte_len = frodoPIR_matrix::matrix_t<lwe_dimension, parsed_db_column_count>::get_byte_len();
  constexpr size_t query_byte_len = frodoPIR_vector::row_vector_t<db_entry_count>::get_byte_len();
  constexpr size_t response_byte_len = frodoPIR_vector::row_vector_t<parsed_db_column_count>::get_byte_len();

  // Database, query and response memory allocation
  std::array<uint8_t, λ / std::numeric_limits<uint8_t>::digits> seed_μ{};
  std::vector<uint8_t> db_bytes(db_byte_len, 0);
  std::vector<uint8_t> pub_matM_bytes(pub_matM_byte_len, 0);
  std::vector<uint8_t> query_bytes(query_byte_len, 0);
  std::vector<uint8_t> response_bytes(response_byte_len, 0);
  std::vector<uint8_t> obtained_db_row_bytes(db_entry_byte_len, 0);

  auto db_bytes_span = std::span<const uint8_t, db_byte_len>(db_bytes);
  auto pub_matM_bytes_span = std::span<uint8_t, pub_matM_byte_len>(pub_matM_bytes);
  auto query_bytes_span = std::span<uint8_t, query_byte_len>(query_bytes);
  auto response_bytes_span = std::span<uint8_t, response_byte_len>(response_bytes);
  auto obtained_db_row_bytes_span = std::span<uint8_t, db_entry_byte_len>(obtained_db_row_bytes);

  prng::prng_t prng{};

  // Sample pseudo random seed
  prng.read(seed_μ);
  // Fill pseudo random database content
  prng.read(db_bytes);

  // Setup the FrodoPIR server
  auto [server, M] = frodoPIR_server::server_t<λ, db_entry_count, db_entry_byte_len, mat_element_bitlen, lwe_dimension>::setup(seed_μ, db_bytes_span);
  M.to_le_bytes(pub_matM_bytes_span);

  // Setup a FrodoPIR client
  auto client = frodoPIR_client::client_t<λ, db_entry_count, db_entry_byte_len, mat_element_bitlen, lwe_dimension>::setup(seed_μ, pub_matM_bytes_span);

  // We will enquire server about the content of this database row
  constexpr size_t to_be_queried_db_row_index = 31;

  // Client preprocesses a query, keeps cached for now; to be used when enquiring content of specified row of the database
  const auto is_query_preprocessed = client.prepare_query(to_be_queried_db_row_index, prng);
  assert(is_query_preprocessed);

  // Client wants to query content of specific database row, for which we've already a query partially prepared
  const auto is_query_ready = client.query(to_be_queried_db_row_index, query_bytes_span);
  assert(is_query_ready);

  // Query reaches FrodoPIR server, it responds back
  server.respond(query_bytes_span, response_bytes_span);

  // Response reaches FrodoPIR client, decodes it, obtains database row content
  const auto is_response_decoded = client.process_response(to_be_queried_db_row_index, response_bytes_span, obtained_db_row_bytes_span);
  assert(is_response_decoded);

  // Original database row content, which server has access to
  const auto orig_db_row_bytes = db_bytes_span.subspan(to_be_queried_db_row_index * db_entry_byte_len, db_entry_byte_len);

  std::cout << "Original database row bytes    : " << to_hex(orig_db_row_bytes) << "\n";
  std::cout << "PIR decoded database row bytes : " << to_hex(obtained_db_row_bytes_span) << "\n";

  // Original database row content and FrodoPIR client decoded row content must match !
  assert(std::ranges::equal(orig_db_row_bytes, obtained_db_row_bytes_span));

  return 0;
}