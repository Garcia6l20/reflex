#pragma once

#if defined(_MSC_VER)
#define REFLEX_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define REFLEX_FORCE_INLINE inline __attribute__((always_inline))
#else
#define REFLEX_FORCE_INLINE inline
#endif
