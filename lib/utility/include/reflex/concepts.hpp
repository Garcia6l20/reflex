#pragma once

#include <concepts>

namespace reflex
{

/** @brief Check given @a T decays to @a U */
template <typename T, typename U>
concept decays_to_c = std::same_as<std::decay_t<T>, U>;

} // namespace reflex
