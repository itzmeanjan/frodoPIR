#pragma once
#include "frodoPIR/internals/matrix/matrix.hpp"
#include "frodoPIR/internals/matrix/serialization.hpp"
#include "frodoPIR/internals/matrix/vector.hpp"
#include "frodoPIR/internals/utility/force_inline.hpp"
#include "frodoPIR/internals/utility/params.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace frodoPIR_server {

// Frodo *P*rivate *I*nformation *R*etrieval Server
template<size_t λ, size_t db_entry_count, size_t db_entry_byte_len, size_t mat_element_bitlen, size_t lwe_dimension>
  requires(frodoPIR_params::check_frodoPIR_params(λ, db_entry_count, mat_element_bitlen, lwe_dimension))
struct server_t
{
public:
  // Compile-time computable values.
  static constexpr auto parsed_db_num_cols = frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen);
  static constexpr auto orig_db_byte_len = db_entry_count * db_entry_byte_len;
  static constexpr auto query_byte_len = db_entry_count * sizeof(frodoPIR_matrix::zq_t);
  static constexpr auto response_byte_len = parsed_db_num_cols * sizeof(frodoPIR_matrix::zq_t);

  // Type aliases.
  using pub_mat_A_t = frodoPIR_matrix::matrix_t<lwe_dimension, db_entry_count>;
  using pub_mat_M_t = frodoPIR_matrix::matrix_t<lwe_dimension, parsed_db_num_cols>;
  using parsed_db_t = frodoPIR_matrix::matrix_t<db_entry_count, parsed_db_num_cols>;
  using query_t = frodoPIR_vector::row_vector_t<db_entry_count>;

  // Constructor(s)
  explicit constexpr server_t(auto db)
    : D(std::move(db))
  {
  }

  server_t(const server_t&) = default;
  server_t(server_t&&) = default;
  server_t& operator=(const server_t&) = default;
  server_t& operator=(server_t&&) = default;

  // Given a `λ` -bit seed and a byte serialized database which has `db_entry_count` -many entries s.t.
  // each entry is of `db_entry_byte_len` -bytes, this routine can be used for setting up FrodoPIR server,
  // returning initialized server (ready to respond to client queries) handle and public matrix M, which will
  // be used by clients for preprocessing queries.
  static forceinline constexpr std::pair<server_t, pub_mat_M_t> setup(std::span<const uint8_t, λ / std::numeric_limits<uint8_t>::digits> seed_μ,
                                                                      std::span<const uint8_t, orig_db_byte_len> db_bytes)
  {
    const auto A = pub_mat_A_t::template generate<λ>(seed_μ);
    const auto D = frodoPIR_serialization::parse_db_bytes<db_entry_count, db_entry_byte_len, mat_element_bitlen>(db_bytes);
    const auto M = A * D;

    return { server_t(D), M };
  }

  // Given byte serialized client query, this routine can be used for responding back to it, producing byte serialized server response.
  constexpr void respond(std::span<const uint8_t, query_byte_len> query_bytes, std::span<uint8_t, response_byte_len> response_bytes) const
  {
    const auto b_tilda = query_t::from_le_bytes(query_bytes);
    const auto c_tilda = b_tilda * this->D;

    c_tilda.to_le_bytes(response_bytes);
  }

private:
  parsed_db_t D{};
};

}
