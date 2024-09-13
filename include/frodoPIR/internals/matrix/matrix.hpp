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
};

}
