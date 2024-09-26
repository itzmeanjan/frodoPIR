#pragma once
#include "frodoPIR/internals/rng/prng.hpp"
#include "frodoPIR/internals/utility/force_inline.hpp"
#include "frodoPIR/internals/utility/utils.hpp"
#include "shake128.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace frodoPIR_matrix {

// All arithmetic operations are performed modulo 2^32, for which we have native reduction.
using zq_t = uint32_t;
constexpr auto Q = static_cast<uint64_t>(std::numeric_limits<zq_t>::max()) + 1;

// Size of interval, used for sampling from uniform ternary distribution χ.
inline constexpr size_t TERNARY_INTERVAL_SIZE = (std::numeric_limits<zq_t>::max() - 2) / 3;
// Uniform sampled value to be rejected, if greater than sampling max, which is < uint32_t_MAX.
inline constexpr size_t TERNARY_REJECTION_SAMPLING_MAX = TERNARY_INTERVAL_SIZE * 3;

// Lambda for computing row width (i.e. number of columns) of parsed database matrix.
constexpr auto get_required_num_columns = [](const size_t db_entry_byte_len, const size_t mat_element_bitlen) {
  const size_t db_entry_bit_len = db_entry_byte_len * std::numeric_limits<uint8_t>::digits;
  const size_t required_num_cols = (db_entry_bit_len + (mat_element_bitlen - 1)) / mat_element_bitlen;

  return required_num_cols;
};

// Matrix of dimension `rows x cols`.
template<size_t rows, size_t cols>
  requires((rows > 0) && (cols > 0))
struct matrix_t
{
public:
  // Constructor(s)
  forceinline constexpr matrix_t() { this->elements = std::vector<zq_t>(rows * cols, zq_t{}); };
  explicit matrix_t(std::vector<zq_t> elements)
    : elements(std::move(elements))
  {
  }
  matrix_t(const matrix_t&) = default;
  matrix_t(matrix_t&&) = default;
  matrix_t& operator=(const matrix_t&) = default;
  matrix_t& operator=(matrix_t&&) = default;

  // Given a `λ` -bit seed, this routine uniform random samples a matrix of dimension `rows x cols`.
  template<size_t λ>
    requires(std::endian::native == std::endian::little)
  static forceinline matrix_t generate(std::span<const uint8_t, λ / std::numeric_limits<uint8_t>::digits> μ)
  {
    constexpr size_t row_byte_len = cols * sizeof(zq_t);

    prng::prng_t prng(μ);
    matrix_t mat{};

    for (size_t r_idx = 0; r_idx < rows; r_idx++) {
      const size_t row_begins_at = r_idx * row_byte_len;

      auto row_begin_ptr = reinterpret_cast<uint8_t*>(mat.elements.data()) + row_begins_at;
      auto row_span = std::span<uint8_t, row_byte_len>(row_begin_ptr, row_byte_len);

      prng.read(row_span);
    }

    return mat;
  }

  // Given a seeded PRNG, this routine can be used for sampling a column vector of size `rows x 1`, s.t. each value is rejection sampled from
  // a uniform ternary distribution χ. Returns sampled value ∈ {-1, 0, +1}.
  //
  // Collects inspiration from https://github.com/brave-experiments/frodo-pir/blob/15573960/src/utils.rs#L102-L125.
  static forceinline constexpr matrix_t sample_from_uniform_ternary_distribution(prng::prng_t& prng)
    requires(cols == 1)
  {
    matrix_t mat{};

    constexpr size_t buffer_byte_len = (8 * shake128::RATE) / std::numeric_limits<uint8_t>::digits;

    std::array<uint8_t, buffer_byte_len> buffer{};
    auto buffer_span = std::span(buffer);

    size_t buffer_offset = 0;
    size_t r_idx = 0;

    while (r_idx < rows) {
      zq_t val = std::numeric_limits<zq_t>::max();

      while (val > TERNARY_REJECTION_SAMPLING_MAX) {
        if ((buffer_offset + sizeof(zq_t)) > buffer_span.size()) {
          const size_t remaining_num_random_bytes = buffer_span.size() - buffer_offset;

          std::copy_n(buffer_span.last(remaining_num_random_bytes).begin(), remaining_num_random_bytes, buffer_span.begin());
          prng.read(buffer_span.subspan(remaining_num_random_bytes));
          buffer_offset = 0;
        }

        val = frodoPIR_utils::from_le_bytes<zq_t>(buffer_span.subspan(buffer_offset, sizeof(zq_t)));
        buffer_offset += sizeof(zq_t);
      }

      zq_t ternary = 0;
      if ((val > TERNARY_INTERVAL_SIZE) && (val <= (2 * TERNARY_INTERVAL_SIZE))) {
        ternary = 1;
      } else if (val > (2 * TERNARY_INTERVAL_SIZE)) {
        ternary = std::numeric_limits<zq_t>::max();
      }

      mat[{ r_idx, 0 }] = ternary;
      r_idx++;
    }

    return mat;
  }

  // Returns an identity matrix of dimension `rows x rows`.
  static forceinline constexpr matrix_t identity()
    requires(rows == cols)
  {
    matrix_t mat{};

    for (size_t idx = 0; idx < rows; idx++) {
      mat[{ idx, idx }] = zq_t(1);
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

  // Get byte length of serialized matrix.
  static forceinline constexpr size_t get_byte_len() { return rows * cols * sizeof(zq_t); }

  // Check equality of two equal dimension matrices, returning boolean result.
  forceinline constexpr bool operator==(const matrix_t& rhs) const
  {
    zq_t result = 0;

    for (size_t r_idx = 0; r_idx < rows; r_idx++) {
      for (size_t c_idx = 0; c_idx < cols; c_idx++) {
        result ^= ((*this)[{ r_idx, c_idx }] ^ rhs[{ r_idx, c_idx }]);
      }
    }

    return (result == 0);
  }

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
  forceinline constexpr matrix_t operator+(const matrix_t& rhs) const
  {
    matrix_t res{};

    for (size_t i = 0; i < rows * cols; i++) {
      res[i] = (*this)[i] + rhs[i];
    }

    return res;
  }

  // Given two matrices A ( of dimension rows x cols ) and B ( of dimension rhs_rows x rhs_cols ) s.t. cols == rhs_rows,
  // this routine can be used for multiplying them over Zq, resulting into another matrix (C) of dimension rows x rhs_cols.
  //
  // Following homebrewed matrix multiplication technique from
  // https://lemire.me/blog/2024/06/13/rolling-your-own-fast-matrix-multiplication-loop-order-and-vectorization.
  template<size_t rhs_rows, size_t rhs_cols>
    requires((cols == rhs_rows))
  forceinline matrix_t<rows, rhs_cols> operator*(const matrix_t<rhs_rows, rhs_cols>& rhs) const
  {
    matrix_t<rows, rhs_cols> res{};

    std::vector<std::thread> threads;
    threads.reserve(rows);

    for (size_t r_idx = 0; r_idx < rows; r_idx++) {
      auto thread = std::thread([=, this, &rhs, &res]() {
        for (size_t k = 0; k < cols; k++) {
          for (size_t c_idx = 0; c_idx < rhs_cols; c_idx++) {
            res[{ r_idx, c_idx }] += (*this)[{ r_idx, k }] * rhs[{ k, c_idx }];
          }
        }
      });

      threads.push_back(std::move(thread));
    }

    for (size_t r_idx = 0; r_idx < rows; r_idx++) {
      threads[r_idx].join();
    }

    return res;
  }

  // Given a matrix M of dimension `rows x cols`, this routine can be used for serializing each of its elements as
  // four little-endian bytes and concatenating them in order to compute a byte array of length `rows * cols * 4`.
  forceinline void to_le_bytes(std::span<uint8_t, matrix_t::get_byte_len()> bytes) const
    requires(std::endian::native == std::endian::little)
  {
    auto elements_ptr = reinterpret_cast<const uint8_t*>(this->elements.data());
    memcpy(bytes.data(), elements_ptr, bytes.size());
  }

  // Given a byte array of length `rows * cols * 4`, this routine can be used for deserializing it as a matrix of dimension
  // `rows x cols` s.t. each matrix element is computed by interpreting four consecutive bytes in little-endian order.
  forceinline static matrix_t from_le_bytes(std::span<const uint8_t, matrix_t::get_byte_len()> bytes)
    requires(std::endian::native == std::endian::little)
  {
    matrix_t res{};

    auto elements_ptr = reinterpret_cast<uint8_t*>(res.elements.data());
    memcpy(elements_ptr, bytes.data(), bytes.size());

    return res;
  }

private:
  std::vector<zq_t> elements;
};

}
