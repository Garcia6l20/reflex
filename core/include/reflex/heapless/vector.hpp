#pragma once

#include <inplace_vector>

namespace reflex::heapless {

template <typename T, std::size_t N> using vector = std::inplace_vector<T, N>;

}