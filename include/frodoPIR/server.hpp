#pragma once
#include "frodoPIR/internals/matrix/matrix.hpp"
#include "frodoPIR/internals/utility/force_inline.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace frodoPIR_server {

// Frodo *P*rivate *I*nformation *R*etrieval Server
template<size_t λ, size_t db_entry_count, size_t db_entry_byte_len, size_t mat_element_bitlen, size_t lwe_dimension>
struct server_t
{
public:
  // Constructor(s)
  explicit constexpr server_t(frodoPIR_matrix::matrix_t<db_entry_count,
                                                        []() {
                                                          const size_t db_entry_bit_len = db_entry_byte_len * std::numeric_limits<uint8_t>::digits;
                                                          const size_t required_num_cols = (db_entry_bit_len + (mat_element_bitlen - 1)) / mat_element_bitlen;

                                                          return required_num_cols;
                                                        }()> db)
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
  static forceinline constexpr std::pair<server_t,
                                         frodoPIR_matrix::matrix_t<lwe_dimension,
                                                                   []() {
                                                                     const size_t db_entry_bit_len = db_entry_byte_len * std::numeric_limits<uint8_t>::digits;
                                                                     return (db_entry_bit_len + (mat_element_bitlen - 1)) / mat_element_bitlen;
                                                                   }()>>
  setup(std::span<const uint8_t, λ / std::numeric_limits<uint8_t>::digits> seed_μ, std::span<const uint8_t, db_entry_count * db_entry_byte_len> db_bytes)
  {
    constexpr auto cols = []() {
      const size_t db_entry_bit_len = db_entry_byte_len * std::numeric_limits<uint8_t>::digits;
      const size_t required_num_cols = (db_entry_bit_len + (mat_element_bitlen - 1)) / mat_element_bitlen;

      return required_num_cols;
    }();

    const auto A = frodoPIR_matrix::matrix_t<lwe_dimension, db_entry_count>::template generate<λ>(seed_μ);
    const auto D = frodoPIR_matrix::matrix_t<db_entry_count, cols>::template parse_db_bytes<db_entry_count, db_entry_byte_len, mat_element_bitlen>(db_bytes);
    const auto M = A * D;

    return { server_t(D), M };
  };

private:
  frodoPIR_matrix::matrix_t<db_entry_count,
                            []() {
                              const size_t db_entry_bit_len = db_entry_byte_len * std::numeric_limits<uint8_t>::digits;
                              const size_t required_num_cols = (db_entry_bit_len + (mat_element_bitlen - 1)) / mat_element_bitlen;

                              return required_num_cols;
                            }()>
    D{};
};

}
