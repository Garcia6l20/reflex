#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <concepts>
#include <meta>
#endif

#include <reflex/meta.hpp>

REFLEX_EXPORT namespace reflex
{
  template <typename E>
  concept enum_c = std::is_enum_v<E> and not std::same_as<E, std::byte>;

  template <typename T>
  concept aggregate_c = std::meta::is_aggregate_type(^^T) and not std::meta::is_array_type(^^T);

  static_assert(not aggregate_c<char[2]>);

  consteval bool decays_to(std::meta::info t, std::meta::info u)
  {
    return decay(t) == u;
  }

  template <typename T, typename U>
  concept decays_to_c = decays_to(^^T, ^^U);

  consteval auto is_int_number_type(std::meta::info t) -> bool
  {
    if(not is_integral_type(t)
       or (t == ^^bool)
       or (t == ^^char)
       //  or (t == ^^signed char)
       //  or (t == ^^unsigned char)
       or (t == ^^char8_t)
       or (t == ^^char16_t)
       or (t == ^^char32_t)
       or (t == ^^wchar_t))
    {
      return false;
    }
    return true;
  }

  template <typename T>
  concept int_number_c = is_int_number_type(^^T);

  template <typename T>
  concept str_c = decays_to_c<T, char*>
               or decays_to_c<T, std::string>
               or decays_to_c<T, std::string_view>
               or requires(T s) { std::string_view(s); };

  template <typename T>
  concept map_c = requires(T t) {
    { t.begin() } -> std::input_iterator;
    { t.end() } -> std::sentinel_for<decltype(t.begin())>;
    typename T::value_type;
    typename T::key_type;
    typename T::mapped_type;
  };

  template <typename T>
  concept seq_c = requires(T t) {
    { std::begin(t) } -> std::input_iterator;
    { std::end(t) } -> std::sentinel_for<decltype(std::begin(t))>;
    { t.size() } -> std::convertible_to<std::size_t>;
    typename T::value_type;
  } and not map_c<T> and not str_c<T>;

  template <typename T>
  concept pair_c = requires(T t) {
    typename T::first_type;
    typename T::second_type;
  };

  template <typename T>
  concept number_c = int_number_c<T> or std::floating_point<T>;

} // namespace reflex
