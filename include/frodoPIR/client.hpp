#pragma once
#include "frodoPIR/internals/matrix/matrix.hpp"
#include "frodoPIR/internals/utility/force_inline.hpp"
#include <cstddef>
#include <cstdint>
#include <utility>

namespace frodoPIR_client {

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

private:
  frodoPIR_matrix::matrix_t<lwe_dimension, db_entry_count> A{};
  frodoPIR_matrix::matrix_t<lwe_dimension, frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen)> M{};
};

}
