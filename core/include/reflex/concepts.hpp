#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <chrono>
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
  concept carray_of_c = std::meta::is_array_type(^^T);

  template <typename T>
  concept array_of_c = meta::is_template_instance_of(decay(^^T), ^^std::array);

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

  // Equality comparability that recurses through containers: libstdc++'s std::vector/std::map
  // operator== are unconstrained templates, so a bare `requires { a == b; }` probe is satisfied
  // even when the element type has no operator== and instantiation would hard-error.
  template <typename A, typename B> consteval bool eq_comparable()
  {
    constexpr bool shallow = requires(A const& a, B const& b) {
      { a == b } -> std::convertible_to<bool>;
    };
    if constexpr(not shallow)
    {
      return false;
    }
    else if constexpr(seq_c<A> and seq_c<B>)
    {
      return eq_comparable<typename A::value_type, typename B::value_type>();
    }
    else if constexpr(map_c<A> and map_c<B>)
    {
      return eq_comparable<typename A::key_type, typename B::key_type>()
         and eq_comparable<typename A::mapped_type, typename B::mapped_type>();
    }
    else
    {
      return true;
    }
  }

  template <typename A, typename B>
  concept eq_comparable_c = eq_comparable<A, B>();

  template <typename T> struct is_optional : std::false_type
  {};
  template <typename T> struct is_optional<std::optional<T>> : std::true_type
  {};
  template <typename T>
  concept optional_c = is_optional<T>::value;

  template <typename T> inline constexpr bool is_chrono_duration_v = false;

  template <typename Rep, typename Period>
  inline constexpr bool is_chrono_duration_v<std::chrono::duration<Rep, Period>> = true;

  /** @brief exactly std::chrono::duration<Rep, Period>, and nothing that merely
   * looks like one
   *
   * A partial specialization rather than a requires-expression naming `T::rep`
   * and `T::period`. Both forms are correct and this one is cheaper: it matches
   * by pattern and instantiates nothing, where the requires form has to form
   * `std::chrono::duration<T::rep, T::period>` for every candidate it is asked
   * about.
   *
   * A GCC 16.1.1 segfault was once attributed to the requires form here. It was
   * not the cause. The trigger is an inline function in a module whose body
   * instantiates a template from a module that one imports, and this concept
   * only ever changed how often that instantiation happened to be reached.
   *
   * Disjoint from parse.hpp's time_point_c either way, which asks for a `clock`
   * a duration does not have.
   */
  template <typename T>
  concept duration_c = is_chrono_duration_v<T>;

} // namespace reflex
