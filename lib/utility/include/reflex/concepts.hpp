#pragma once

#include <concepts>
#include <type_traits>

namespace reflex
{

/** @brief Check given @a T decays to @a U */
template <typename T, typename U>
concept decays_to_c = std::same_as<std::decay_t<T>, U>;

/** @brief Check given @a T DOES NOT decays to @a U */
template <typename T, typename U>
concept non_decays_to_c = not decays_to_c<T, U>;

#define auto_of(__type) reflex::decays_to_c<__type> auto&&

#define auto_not_of(__type) reflex::non_decays_to_c<__type> auto&&

template <typename F, auto value = std::bool_constant<(F{}(), true)>()> consteval bool is_constexpr(F)
{
  return value;
}

template <typename T, typename... Args>
concept constexpr_constructible_c = requires(Args&&... args) { is_constexpr([] { T{std::forward<Args...>(args)...}; }); };

} // namespace reflex
