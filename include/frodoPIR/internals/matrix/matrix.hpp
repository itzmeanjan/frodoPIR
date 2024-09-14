#pragma once
#include "frodoPIR/internals/rng/prng.hpp"
#include "frodoPIR/internals/utility/force_inline.hpp"
#include "frodoPIR/internals/utility/utils.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace frodoPIR_matrix {

// All arithmetic operations are performed modulo 2^32, for which we have native reduction.
using zq_t = uint32_t;

// Matrix of dimension `rows x cols`.
template<size_t rows, size_t cols>
  requires((rows > 0) && (cols > 0))
struct matrix_t
{
private:
  std::array<zq_t, rows * cols> elements{};

public:
  // Constructor(s)
  forceinline constexpr matrix_t() = default;
  // Given a `λ` -bit seed, this routine uniform random samples a matrix of dimension `rows x cols`.
  template<size_t λ>
  static forceinline constexpr matrix_t generate(std::span<const uint8_t, λ / std::numeric_limits<uint8_t>::digits> μ)
  {
    prng::prng_t prng(μ);

    std::array<uint8_t, sizeof(zq_t)> buffer{};
    auto buffer_span = std::span(buffer);

    matrix_t mat{};

    for (size_t r_idx = 0; r_idx < rows; r_idx++) {
      for (size_t c_idx = 0; c_idx < cols; c_idx++) {
        prng.read(buffer_span);
        mat[{ r_idx, c_idx }] = frodoPIR_utils::from_le_bytes<zq_t>(buffer_span);
      }
    }

    return mat;
  }

  // Given a byte serialized database s.t. it has `db_entry_count` -number of rows and each row contains `db_entry_byte_len` -bytes
  // entry, this routines parses database into a matrix s.t. each element of matrix has `mat_element_bitlen` significant bits.
  //
  // Note, 0 < `mat_element_bitlen` < 32.
  // Collects inspiration from https://github.com/brave-experiments/frodo-pir/blob/15573960/src/db.rs#L229-L254.
  template<size_t db_entry_count, size_t db_entry_byte_len, size_t mat_element_bitlen>
    requires((rows == db_entry_count) &&
             (cols ==
              []() {
                const size_t db_entry_bit_len = db_entry_byte_len * std::numeric_limits<uint8_t>::digits;
                const size_t required_num_cols = (db_entry_bit_len + (mat_element_bitlen - 1)) / mat_element_bitlen;

                return required_num_cols;
              }()) &&
             ((0 < mat_element_bitlen) && (mat_element_bitlen < std::numeric_limits<zq_t>::digits)))
  static forceinline matrix_t parse_db_bytes(std::span<const uint8_t, db_entry_count * db_entry_byte_len> bytes)
  {
    constexpr auto mat_element_mask = (1ul << mat_element_bitlen) - 1ul;

    matrix_t mat{};
    size_t mat_lin_idx = 0;

    uint64_t buffer = 0;
    auto buffer_span = std::span<uint8_t, sizeof(buffer)>(reinterpret_cast<uint8_t*>(&buffer), sizeof(buffer));

    size_t buf_num_bits = 0;
    size_t byte_off = 0;

    while (byte_off < bytes.size()) {
      const size_t remaining_num_bytes = bytes.size() - byte_off;

      const size_t fillable_num_bits = std::numeric_limits<decltype(buffer)>::digits - buf_num_bits;
      const size_t readable_num_bits = fillable_num_bits & (-std::numeric_limits<uint8_t>::digits);
      const size_t readable_num_bytes = std::min(readable_num_bits / std::numeric_limits<uint8_t>::digits, remaining_num_bytes);

      const auto read_word = frodoPIR_utils::from_le_bytes<uint64_t>(bytes.subspan(byte_off, readable_num_bytes));
      byte_off += readable_num_bytes;

      buffer |= (read_word << buf_num_bits);
      buf_num_bits += readable_num_bits;

      const size_t fillable_mat_elem_count = buf_num_bits / mat_element_bitlen;

      for (size_t elem_idx = 0; elem_idx < fillable_mat_elem_count; elem_idx++) {
        const zq_t mat_element = buffer & mat_element_mask;
        mat[mat_lin_idx + elem_idx] = mat_element;

        buffer >>= mat_element_bitlen;
        buf_num_bits -= mat_element_bitlen;
      }

      mat_lin_idx += fillable_mat_elem_count;
    }

    return mat;
  }

  // Accessor, using {row_index, column_index} pair.
  forceinline constexpr zq_t& operator[](const std::pair<size_t, size_t> idx)
  {
    const auto [r_idx, c_idx] = idx;
    return this->elements[r_idx * cols + c_idx];
  }
  forceinline constexpr const zq_t& operator[](const std::pair<size_t, size_t> idx) const
  {
    const auto [r_idx, c_idx] = idx;
    return this->elements[r_idx * cols + c_idx];
  }

  // Accessor, using linearized index.
  forceinline constexpr zq_t& operator[](const size_t lin_idx) { return this->elements[lin_idx]; }
  forceinline constexpr const zq_t& operator[](const size_t lin_idx) const { return this->elements[lin_idx]; }

  // Given a matrix M of dimension `rows x cols`, this routine is used for computing its transpose M' s.t.
  // resulting matrix's dimension becomes `cols x rows`.
  forceinline constexpr matrix_t<cols, rows> transpose() const
  {
    matrix_t<cols, rows> res{};

    for (size_t i = 0; i < cols; i++) {
      for (size_t j = 0; j < rows; j++) {
        res[{ i, j }] = (*this)[{ j, i }];
      }
    }

    return res;
  }

  // Given two matrices A, B of equal dimension, this routine can be used for performing matrix addition over Zq,
  // returning a matrix of same dimension.
  forceinline constexpr matrix_t operator+(const matrix_t<rows, cols>& rhs) const
  {
    matrix_t res{};

    for (size_t i = 0; i < rows * cols; i++) {
      res[i] = (*this)[i] + rhs[i];
    }

    return res;
  }

  // Given two matrices A ( of dimension rows x cols ) and B ( of dimension rhs_rows x rhs_cols ) s.t. cols == rhs_rows,
  // this routine can be used for multiplying them over Zq, resulting into another matrix (C) of dimension rows x rhs_cols.
  template<size_t rhs_rows, size_t rhs_cols>
    requires((cols == rhs_rows))
  forceinline constexpr matrix_t<rows, rhs_cols> operator*(const matrix_t<rhs_rows, rhs_cols>& rhs) const
  {
    matrix_t<rows, rhs_cols> res{};

    for (size_t r_idx = 0; r_idx < rows; r_idx++) {
      for (size_t c_idx = 0; c_idx < rhs_cols; c_idx++) {
        zq_t tmp{};

        for (size_t k = 0; k < cols; k++) {
          tmp += (*this)[{ r_idx, k }] * rhs[{ k, c_idx }];
        }

        res[{ r_idx, c_idx }] = tmp;
      }
    }

    return res;
  }

  // Given a matrix M of dimension `rows x cols`, this routine can be used for serializing each of its elements as
  // four little-endian bytes and concatenating them in order to compute a byte array of length m * n * 4.
  forceinline constexpr void to_le_bytes(std::span<uint8_t, rows * cols * sizeof(zq_t)> bytes) const
  {
    for (size_t i = 0; i < rows * cols; i++) {
      const size_t boff = i * sizeof(zq_t);

      const auto word = (*this)[i];

      bytes[boff + 0] = static_cast<uint8_t>((word >> 0u) & 0xffu);
      bytes[boff + 1] = static_cast<uint8_t>((word >> 8u) & 0xffu);
      bytes[boff + 2] = static_cast<uint8_t>((word >> 16u) & 0xffu);
      bytes[boff + 3] = static_cast<uint8_t>((word >> 24u) & 0xffu);
    }
  }

  // Given a byte array of length m * n * 4, this routine can be used for deserializing it as a matrix of dimension m x n
  // s.t. each matrix element is computed by interpreting four consecutive bytes in little-endian order.
  forceinline static matrix_t from_le_bytes(std::span<const uint8_t, rows * cols * sizeof(zq_t)> bytes)
  {
    constexpr size_t blen = bytes.size();
    matrix_t res{};

    size_t boff = 0;
    size_t lin_idx = 0;

    while (boff < blen) {
      const zq_t word = ((static_cast<zq_t>(bytes[boff + 3]) << 24u) | (static_cast<zq_t>(bytes[boff + 2]) << 16u) |
                         (static_cast<zq_t>(bytes[boff + 1]) << 8u) | (static_cast<zq_t>(bytes[boff + 0]) << 0u));

      res[lin_idx] = word;

      boff += sizeof(zq_t);
      lin_idx += 1;
    }

    return res;
  }
};

}
