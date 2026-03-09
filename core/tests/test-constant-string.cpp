#include <doctest/doctest.h>

import reflex.core;

using namespace reflex;

template <constant_string S> struct use_string
{
  static constexpr constant_string s = S;
};

using namespace std::literals;

TEST_CASE("reflex::constant_string: basics")
{
  static constexpr auto from_lit = use_string<"hello">::s;
  static_assert(from_lit == "hello");
  static constexpr auto from_sv = use_string<"hello"sv>::s;
  static_assert(from_sv == "hello");
  static constexpr auto from_s = use_string<"hello"s>::s;
  static_assert(from_s == "hello");

  static_assert(from_s.data() == from_lit.data());
  static_assert(from_sv.data() == from_lit.data());
}

struct use_string_array
{
  std::span<const constant_string> arr;
};

consteval constant_string operator""_sc(const char* data, std::size_t N)
{
  return {
      std::string_view{data, N}
  };
}

TEST_CASE("reflex::constant_string: string array")
{
  static constexpr auto tmp = "hello"_sc;
  static_assert(tmp == "hello");
  // static constexpr auto tmp2 = std::array{"hello"_sc, "world"_sc};
  // static constexpr auto tmp3 = define_static_array(tmp2);
  // static constexpr auto from_arr = use_string_array{tmp3};
}