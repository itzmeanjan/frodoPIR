#include "frodoPIR/internals/matrix/matrix.hpp"
#include "frodoPIR/internals/rng/prng.hpp"
#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <vector>

template<size_t rows, size_t cols>
static void
test_matrix_operations()
{
  constexpr size_t λ = 128;
  constexpr size_t mat_byte_len = frodoPIR_matrix::matrix_t<rows, cols>::get_byte_len();

  std::array<uint8_t, λ / std::numeric_limits<uint8_t>::digits> μ{};
  std::vector<uint8_t> matA_bytes(mat_byte_len, 0);

  auto μ_span = std::span(μ);
  auto matA_bytes_span = std::span<uint8_t, mat_byte_len>(matA_bytes);

  prng::prng_t prng;
  prng.read(μ_span);

  auto A = frodoPIR_matrix::matrix_t<rows, cols>::template generate<λ>(μ_span);
  auto A_transposed = A.transpose();
  A_transposed.to_le_bytes(matA_bytes_span);

  auto A_prime = frodoPIR_matrix::matrix_t<cols, rows>::from_le_bytes(matA_bytes_span);
  auto A_prime_transposed = A_prime.transpose();

  EXPECT_EQ(A, A_prime_transposed);

  auto I = frodoPIR_matrix::matrix_t<cols, cols>::identity();
  auto AI = A * I;

  EXPECT_EQ(A, AI);

  auto I_prime = frodoPIR_matrix::matrix_t<rows, rows>::identity();
  auto IA = I_prime * A;

  EXPECT_EQ(A, IA);
}

TEST(FrodoPIR, MatrixOperations)
{
  test_matrix_operations<1774, 1ul << 16>();
  test_matrix_operations<1774, 1ul << 20>();
}
