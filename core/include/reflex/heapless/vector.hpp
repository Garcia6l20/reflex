#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <inplace_vector>
#endif

REFLEX_EXPORT namespace reflex::heapless
{
  template <typename T, std::size_t N> using vector = std::inplace_vector<T, N>;
}