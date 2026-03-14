export module reflex.core:concepts;

import std;

export namespace reflex
{

template <typename E>
concept enum_c = std::is_enum_v<E>;

template <typename T>
concept aggregate_c = std::meta::is_aggregate_type(^^T) and not std::meta::is_array_type(^^T);

static_assert(not aggregate_c<char[2]>);

template <typename T, typename U>
concept decays_to_c = std::same_as<std::decay_t<T>, U>;

consteval auto is_int_number_type(std::meta::info t) -> bool
{
  if(not is_integral_type(t)
     or (t == ^^bool)
     or (t == ^^char)
     or (t == ^^signed char)
     or (t == ^^unsigned char)
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

} // namespace reflex
