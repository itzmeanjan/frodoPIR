#pragma once
#include "frodoPIR/internals/rng/prng.hpp"
#include "frodoPIR/internals/utility/force_inline.hpp"
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
  forceinline matrix_t generate(std::span<const uint8_t, λ / std::numeric_limits<uint8_t>::digits> μ)
  {
    prng::prng_t prng(μ);

    zq_t word{};
    matrix_t mat{};

    for (size_t r_idx = 0; r_idx < rows; r_idx++) {
      for (size_t c_idx = 0; c_idx < cols; c_idx++) {
        prng.read(std::span<uint8_t, sizeof(word)>(reinterpret_cast<uint8_t*>(&word), sizeof(word)));
        mat[{ r_idx, c_idx }] = word;
      }
    }

    return mat;
  }

  // Accessor(s)
  forceinline constexpr zq_t& operator[](std::pair<size_t, size_t> idx)
  {
    const auto [r_idx, c_idx] = idx;
    return this->elements[r_idx * cols + c_idx];
  }
  forceinline constexpr const zq_t& operator[](std::pair<size_t, size_t> idx) const
  {
    const auto [r_idx, c_idx] = idx;
    return this->elements[r_idx * cols + c_idx];
  }

  // Given two matrices A ( of dimension rows x cols ) and B ( of dimension rhs_rows x rhs_cols ) s.t. cols == rhs_rows,
  // this routine can be used for multiplying them over Zq, resulting into another matrix (C) of dimension rows x rhs_cols.
  template<size_t rhs_rows, size_t rhs_cols>
    requires((cols == rhs_rows))
  forceinline constexpr matrix_t<rows, rhs_cols> operator*(const matrix_t<rhs_rows, rhs_cols>& rhs)
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
};

}
