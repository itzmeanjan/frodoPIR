#pragma once
#include "frodoPIR/internals/matrix/matrix.hpp"
#include "frodoPIR/internals/matrix/serialization.hpp"
#include "frodoPIR/internals/matrix/vector.hpp"
#include "frodoPIR/internals/rng/prng.hpp"
#include "frodoPIR/internals/utility/force_inline.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>

namespace frodoPIR_client {

// FrodoPIR query status
enum class query_status_t : uint32_t
{
  prepared,
  sent,
};

// FrodoPIR client query data type
template<size_t db_entry_count, size_t db_entry_byte_len, size_t mat_element_bitlen>
struct client_query_t
{
  query_status_t status;
  size_t db_index;
  frodoPIR_vector::row_vector_t<db_entry_count> b;
  frodoPIR_vector::row_vector_t<frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen)> c;
};

// Frodo *P*rivate *I*nformation *R*etrieval Client
template<size_t λ, size_t db_entry_count, size_t db_entry_byte_len, size_t mat_element_bitlen, size_t lwe_dimension>
struct client_t
{
public:
  // Constructor(s)
  explicit constexpr client_t(
    frodoPIR_matrix::matrix_t<lwe_dimension, db_entry_count> pub_matA,
    frodoPIR_matrix::matrix_t<lwe_dimension, frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen)> pub_matM)
    : A(std::move(pub_matA))
    , M(std::move(pub_matM))
  {
  }

  client_t(const client_t&) = default;
  client_t(client_t&&) = default;
  client_t& operator=(const client_t&) = default;
  client_t& operator=(client_t&&) = default;

  // Given a `λ` -bit seed and a byte serialized public matrix M, computed by frodoPIR server, this routine can be used
  // for setting up FrodoPIR client, ready to generate queries and process server response.
  static forceinline constexpr client_t setup(
    std::span<const uint8_t, λ / std::numeric_limits<uint8_t>::digits> seed_μ,
    std::span<const uint8_t, lwe_dimension * frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen) * sizeof(frodoPIR_matrix::zq_t)>
      pub_matM_bytes)
  {
    constexpr auto cols = frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen);

    return client_t(frodoPIR_matrix::matrix_t<lwe_dimension, db_entry_count>::template generate<λ>(seed_μ),
                    frodoPIR_matrix::matrix_t<lwe_dimension, cols>::from_le_bytes(pub_matM_bytes));
  }

  // Given a set of database row indices, this routine prepares those many queries, for enquiring their values,
  // using FrodoPIR scheme.
  constexpr void prepare_query(std::span<const size_t> db_row_indices, prng::prng_t& prng)
  {
    for (const auto db_row_index : db_row_indices) {
      const auto s = frodoPIR_vector::column_vector_t<lwe_dimension>::sample_from_uniform_ternary_distribution(prng);  // secret vector
      const auto e = frodoPIR_vector::column_vector_t<db_entry_count>::sample_from_uniform_ternary_distribution(prng); // error vector

      const auto s_transposed = s.transpose();
      const auto b = s_transposed * this->A + e.transpose();
      const auto c = s_transposed * this->M;

      this->queries[db_row_index] = client_query_t<db_entry_count, db_entry_byte_len, mat_element_bitlen>{
        .status = query_status_t::prepared,
        .db_index = db_row_index,
        .b = b,
        .c = c,
      };
    }
  }

  // Given a database row index, for which query has already been prepared, this routine finalizes the query,
  // making it ready for processing at the server's end.
  constexpr bool query(const size_t db_row_index, std::span<uint8_t, db_entry_count * sizeof(frodoPIR_matrix::zq_t)> query_bytes)
  {
    if (!this->queries.contains(db_row_index)) {
      return false;
    }
    if (this->queries[db_row_index].status != query_status_t::prepared) {
      return false;
    }

    constexpr auto rho = 1ul << mat_element_bitlen;
    constexpr auto query_indicator_value = static_cast<frodoPIR_matrix::zq_t>(frodoPIR_matrix::Q / rho);

    this->queries[db_row_index].b[db_row_index] += query_indicator_value;
    this->queries[db_row_index].b.to_le_bytes(query_bytes);
    this->queries[db_row_index].status = query_status_t::sent;

    return true;
  }

  // Given a database row index, for which query has already been sent to server and we are awaiting response,
  // this routine can be used for processing server response, returning byte serialized content of queried row.
  constexpr bool process_response(
    const size_t db_row_index,
    std::span<const uint8_t, frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen) * sizeof(frodoPIR_matrix::zq_t)> response_bytes,
    std::span<uint8_t, db_entry_byte_len> db_row_bytes)
  {
    if (!this->queries.contains(db_row_index)) {
      return false;
    }
    if (this->queries[db_row_index].status != query_status_t::sent) {
      return false;
    }

    constexpr auto rho = 1ul << mat_element_bitlen;
    constexpr auto rounding_factor = static_cast<frodoPIR_matrix::zq_t>(frodoPIR_matrix::Q / rho);
    constexpr auto rounding_floor = rounding_factor / 2;

    constexpr size_t db_matrix_row_width = frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen);

    frodoPIR_vector::row_vector_t<db_matrix_row_width> db_matrix_row{};
    auto c_tilda = frodoPIR_vector::row_vector_t<db_matrix_row_width>::from_le_bytes(response_bytes);

    for (size_t idx = 0; idx < db_matrix_row_width; idx++) {
      const auto unscaled_res = c_tilda[idx] - this->queries[db_row_index].c[idx];

      const auto scaled_res = unscaled_res / rounding_factor;
      const auto scaled_rem = unscaled_res % rounding_factor;

      auto rounded_res = scaled_res;

      rounded_res += ((scaled_rem > rounding_floor) ? 1 : 0);
      rounded_res %= static_cast<frodoPIR_matrix::zq_t>(rho);

      db_matrix_row[idx] = rounded_res;
    }

    frodoPIR_serialization::serialize_db_row<db_entry_byte_len, mat_element_bitlen>(db_matrix_row, db_row_bytes);
    this->queries.erase(db_row_index);

    return true;
  }

private:
  frodoPIR_matrix::matrix_t<lwe_dimension, db_entry_count> A{};
  frodoPIR_matrix::matrix_t<lwe_dimension, frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen)> M{};
  std::unordered_map<size_t, client_query_t<db_entry_count, db_entry_byte_len, mat_element_bitlen>> queries{};
};

}
