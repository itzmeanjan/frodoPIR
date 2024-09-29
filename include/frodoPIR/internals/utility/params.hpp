#pragma once
#include "frodoPIR/internals/matrix/matrix.hpp"
#include <cmath>
#include <cstddef>

namespace frodoPIR_params {

// Compile-time check, if chosen parameters for instantiating FrodoPIR, is correct, following Eq. 8 in section 5.1 of https://ia.cr/2022/981.
consteval bool
check_frodoPIR_param_correctness(const size_t db_entry_count, const size_t mat_element_bitlen)
{
  const auto ρ = 1ul << mat_element_bitlen;
  return frodoPIR_matrix::Q >= ((8 * ρ * ρ) * std::sqrt(db_entry_count));
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
