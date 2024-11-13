#include "frodoPIR/internals/matrix/matrix.hpp"
#include "randomshake/randomshake.hpp"
#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <vector>

TEST(FrodoPIR, MatrixOperations)
{
  constexpr size_t λ = 128;
  constexpr size_t rows = 1024;
  constexpr size_t cols = rows + 1;

  constexpr size_t mat_byte_len = frodoPIR_matrix::matrix_t<rows, cols>::get_byte_len();

  std::array<uint8_t, λ / std::numeric_limits<uint8_t>::digits> μ{};
  std::vector<uint8_t> matA_bytes(mat_byte_len, 0);

  auto μ_span = std::span(μ);
  auto matA_bytes_span = std::span<uint8_t, mat_byte_len>(matA_bytes);

  randomshake::randomshake_t<128> csprng;
  csprng.generate(μ_span);

  auto A = frodoPIR_matrix::matrix_t<rows, cols>::template generate<λ>(μ_span);
  A.to_le_bytes(matA_bytes_span);

  {
    auto A_prime = frodoPIR_matrix::matrix_t<rows, cols>::from_le_bytes(matA_bytes_span);
    EXPECT_EQ(A, A_prime);
  }

  {
    auto I = frodoPIR_matrix::matrix_t<cols, cols>::identity();
    auto AI = A * I;

    EXPECT_EQ(A, AI);
  }

  {
    auto I_prime = frodoPIR_matrix::matrix_t<rows, rows>::identity();
    auto IA = I_prime * A;

    EXPECT_EQ(A, IA);
  }
}
