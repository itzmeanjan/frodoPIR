#pragma once
#include "frodoPIR/internals/matrix/matrix.hpp"
#include <cstddef>

namespace frodoPIR_params {

// Compile-time executable recursive integer square-root function.
constexpr size_t
ct_sqrt_helper(const size_t x, const size_t lo, const size_t hi)
{
  if (lo == hi) {
    return lo;
  }

  const auto mid = (lo + hi + 1ul) / 2ul;
  return ((x / mid) < mid) ? ct_sqrt_helper(x, lo, mid - 1ul) : ct_sqrt_helper(x, mid, hi);
}

// Homebrewed compile-time executable square-root implementation for integer values.
// Taken from https://stackoverflow.com/a/27709195.
constexpr size_t
ct_sqrt(const size_t x)
{
  return ct_sqrt_helper(x, 0, (x / 2ul) + 1ul);
}

// Compile-time check, if chosen parameters for instantiating FrodoPIR, is correct, following Eq. 8 in section 5.1 of https://ia.cr/2022/981.
consteval bool
check_frodoPIR_param_correctness(const size_t db_entry_count, const size_t mat_element_bitlen)
{
  const auto ρ = 1ul << mat_element_bitlen;
  return frodoPIR_matrix::Q >= ((8 * ρ * ρ) * ct_sqrt(db_entry_count));
}

// Compile-time check, if instantiated FrodoPIR uses one of recommended parameters in table 5 of https://ia.cr/2022/981.
consteval bool
check_frodoPIR_params(const size_t λ, const size_t db_entry_count, const size_t mat_element_bitlen, const size_t lwe_dimension)
{
  return check_frodoPIR_param_correctness(db_entry_count, mat_element_bitlen) && // First check if Eq. 8 of https://ia.cr/2022/981 holds
         (λ == 128) &&                                                           // Only 128 -bit security level is supported
         (lwe_dimension == 1774) &&                                              // LWE dimension is also fixed for all variants
         (((db_entry_count == (1ul << 16)) && (mat_element_bitlen == 10)) ||     // Database has 2^16 rows s.t. each row can be of arbitrary byte length
          ((db_entry_count == (1ul << 17)) && (mat_element_bitlen == 10)) ||     // Database has 2^17 rows s.t. each row can be of arbitrary byte length
          ((db_entry_count == (1ul << 18)) && (mat_element_bitlen == 10)) ||     // Database has 2^18 rows s.t. each row can be of arbitrary byte length
          ((db_entry_count == (1ul << 19)) && (mat_element_bitlen == 9)) ||      // Database has 2^19 rows s.t. each row can be of arbitrary byte length
          ((db_entry_count == (1ul << 20)) && (mat_element_bitlen == 9))         // Database has 2^20 rows s.t. each row can be of arbitrary byte length
         );
}

}
