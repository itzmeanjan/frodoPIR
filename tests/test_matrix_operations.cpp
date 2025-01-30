#include "frodoPIR/internals/matrix/matrix.hpp"
#include "frodoPIR/internals/matrix/vector.hpp"
#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <vector>

TEST(FrodoPIR, MatrixMultiplicationWorks)
{
  constexpr size_t λ = 128;
  constexpr size_t rows = 1024;
  constexpr size_t cols = rows + 1;

  std::array<uint8_t, λ / std::numeric_limits<uint8_t>::digits> μ{};
  auto μ_span = std::span(μ);

  csprng::csprng_t csprng;
  csprng.generate(μ_span);

  auto A = frodoPIR_matrix::matrix_t<rows, cols>::template generate<λ>(μ_span);

  auto I = frodoPIR_matrix::matrix_t<cols, cols>::identity();
  auto AI = A * I;
  EXPECT_EQ(A, AI);

  auto I_prime = frodoPIR_matrix::matrix_t<rows, rows>::identity();
  auto IA = I_prime * A;
  EXPECT_EQ(A, IA);
}

TEST(FrodoPIR, MatrixSerializationWorks)
{
  constexpr size_t λ = 128;
  constexpr size_t rows = 1024;
  constexpr size_t cols = rows + 1;

  constexpr size_t mat_byte_len = frodoPIR_matrix::matrix_t<rows, cols>::get_byte_len();

  std::array<uint8_t, λ / std::numeric_limits<uint8_t>::digits> μ{};
  std::vector<uint8_t> matA_bytes(mat_byte_len, 0);

  auto μ_span = std::span(μ);
  auto matA_bytes_span = std::span<uint8_t, mat_byte_len>(matA_bytes);

  csprng::csprng_t csprng;
  csprng.generate(μ_span);

  auto A = frodoPIR_matrix::matrix_t<rows, cols>::template generate<λ>(μ_span);
  A.to_le_bytes(matA_bytes_span);

  auto A_prime = frodoPIR_matrix::matrix_t<rows, cols>::from_le_bytes(matA_bytes_span);
  EXPECT_EQ(A, A_prime);
}

TEST(FrodoPIR, RowVectorMatrixMultiplicationWorks)
{
  constexpr size_t λ = 128;
  constexpr size_t cols = 1024;

  std::array<uint8_t, λ / std::numeric_limits<uint8_t>::digits> μ{};
  auto μ_span = std::span(μ);

  csprng::csprng_t csprng;
  csprng.generate(μ_span);

  auto row_vector = frodoPIR_vector::row_vector_t<cols>::template generate<λ>(μ_span);
  auto I = frodoPIR_matrix::matrix_t<cols, cols>::identity();

  auto row_vector_prime = row_vector.row_vector_x_transposed_matrix(I);
  EXPECT_EQ(row_vector, row_vector_prime);
}

TEST(FrodoPIR, MatrixTranspositionWorks)
{
  constexpr size_t λ = 128;
  constexpr size_t rows = 1024;
  constexpr size_t cols = rows + 1;

  std::array<uint8_t, λ / std::numeric_limits<uint8_t>::digits> μ{};
  auto μ_span = std::span(μ);

  csprng::csprng_t csprng;
  csprng.generate(μ_span);

  auto A = frodoPIR_matrix::matrix_t<rows, cols>::template generate<λ>(μ_span);
  auto A_transposed = A.transpose();
  auto A_transposed_transposed = A_transposed.transpose();

  EXPECT_EQ(A, A_transposed_transposed);
}
