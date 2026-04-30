#pragma once


#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <type_traits>
#endif

namespace reflex
{
/// @brief A type that can be used to indicate that a particular value should be ignored by reflex
/// default operations, such as parsing and formatting.
template <typename T> struct ignore : std::false_type
{};

template <typename T>
concept ignored_c = ignore<std::remove_cvref_t<T>>::value;
} // namespace reflex
