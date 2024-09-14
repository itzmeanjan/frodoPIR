#pragma once
#include "frodoPIR/internals/utility/force_inline.hpp"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace frodoPIR_utils {

// Given a byte array of length n (>=0), this routine copies input bytes into destination word, of unsigned type T,
// while placing bytes following little-endian ordering.
template<typename T>
forceinline constexpr T
from_le_bytes(std::span<const uint8_t> bytes)
  requires(std::is_unsigned_v<T> && (std::endian::native == std::endian::little))
{
  T res{};

  const size_t copyable = std::min(sizeof(res), bytes.size());
  for (size_t i = 0; i < copyable; i++) {
    res |= (static_cast<T>(bytes[i]) << (i * std::numeric_limits<uint8_t>::digits));
  }

  return res;
}

// Given an unsigned integer as input, this routine copies source bytes, following little-endian order, into destination
// byte array of length n (>=0).
forceinline constexpr void
to_le_bytes(const std::unsigned_integral auto v, std::span<uint8_t> bytes)
  requires(std::endian::native == std::endian::little)
{
  const size_t copyable = std::min(sizeof(v), bytes.size());
  for (size_t i = 0; i < copyable; i++) {
    bytes[i] = static_cast<uint8_t>(v >> (i * std::numeric_limits<uint8_t>::digits));
  }
}

}
