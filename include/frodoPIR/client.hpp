#pragma once
#include "frodoPIR/internals/matrix/matrix.hpp"
#include "frodoPIR/internals/matrix/vector.hpp"
#include "frodoPIR/internals/rng/prng.hpp"
#include "frodoPIR/internals/utility/force_inline.hpp"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>

namespace frodoPIR_client {

// FrodoPIR query status
enum class query_status_t : uint32_t
{
  prepared,
  sent,
  responded
};

// FrodoPIR client query data type
template<size_t db_entry_count, size_t db_entry_byte_len, size_t mat_element_bitlen>
struct client_query_t
{
  query_status_t status;
  size_t db_index;
  frodoPIR_vector::vector_t<db_entry_count> b;
  frodoPIR_vector::vector_t<frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen)> c;
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
    std::span<const uint8_t, lwe_dimension * frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen)> pub_matM_bytes)
  {
    constexpr auto cols = frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen);

    return client_t(frodoPIR_matrix::matrix_t<lwe_dimension, db_entry_count>::template generate<λ>(seed_μ),
                    frodoPIR_matrix::matrix_t<lwe_dimension, cols>::from_le_bytes(pub_matM_bytes));
  }

  // Given a set of database row indices, this routine prepares those many queries, for enquiring their values, using FrodoPIR scheme.
  constexpr void prepare_query(std::span<const size_t> db_indices, prng::prng_t& prng)
  {
    for (const auto db_index : db_indices) {
      const auto s = frodoPIR_vector::vector_t<lwe_dimension>::sample_from_uniform_ternary_distribution(prng);  // secret vector
      const auto e = frodoPIR_vector::vector_t<db_entry_count>::sample_from_uniform_ternary_distribution(prng); // error vector

      const auto s_transposed = s.transpose();
      const auto b = s_transposed * this->A + e.transpose();
      const auto c = s_transposed * this->M;

      this->queries[db_index] = client_query_t<db_entry_count, db_entry_byte_len, mat_element_bitlen>{
        .status = query_status_t::prepared,
        .db_index = db_index,
        .b = b,
        .c = c,
      };
    }
  }

private:
  frodoPIR_matrix::matrix_t<lwe_dimension, db_entry_count> A{};
  frodoPIR_matrix::matrix_t<lwe_dimension, frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen)> M{};
  std::unordered_map<size_t, client_query_t<db_entry_count, db_entry_byte_len, mat_element_bitlen>> queries{};
};

}
