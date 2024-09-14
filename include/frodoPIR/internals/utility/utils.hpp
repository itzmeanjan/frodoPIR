#pragma once
#include "frodoPIR/internals/utility/force_inline.hpp"
#include <bit>
#include <cstddef>

namespace frodoPIR_utils {

// Given an unsigned integer as input, this routine returns TRUTH value only if `v` is power of 2, otherwise it returns FALSE.
template<typename T>
forceinline constexpr bool
is_power_of_2(const T v)
  requires(std::is_unsigned_v<T>)
{
  return ((v) & (v - 1)) == 0;
}

// Given a power of 2 value `v`, this routine returns logarithm base-2 of v.
template<size_t v>
forceinline constexpr size_t
log2()
  requires((v > 0) && is_power_of_2(v))
{
  return std::countr_zero(v);
}

}
