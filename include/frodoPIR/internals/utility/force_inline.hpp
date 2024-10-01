#pragma once

// Following content is taken from https://github.com/itzmeanjan/raccoon/blob/c66194d5/include/raccoon/internals/utility/force_inline.hpp

#ifdef _MSC_VER
// MSVC
#define forceinline __forceinline

#elif defined(__GNUC__)
// GCC
#if defined(__cplusplus) && __cplusplus >= 201103L
#define forceinline inline __attribute__((__always_inline__))
#else
#define forceinline inline
#endif

#elif defined(__CLANG__)
// Clang
#if __has_attribute(__always_inline__)
#define forceinline inline __attribute__((__always_inline__))
#else
#define forceinline inline
#endif

#else
// Others
#define forceinline inline

#endif
