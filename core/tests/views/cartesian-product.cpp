#include <doctest/doctest.h>

import reflex.core;

using namespace reflex;

  template<std::ranges::input_range _Rg>
    consteval std::span<const std::ranges::range_value_t<_Rg>>
    test_define_static_array(_Rg&& __r)
    {
      using _Tp = std::ranges::range_value_t<_Rg>;
      auto __array = std::meta::reflect_constant_array(__r);
      auto __type = std::meta::type_of(__array);
      if (std::meta::is_array_type(__type))
	return std::span<const _Tp>(std::meta::extract<const _Tp*>(__array),
			       std::meta::extent(__type, 0U));
      else {
        throw std::logic_error("test_define_static_array: not an array type");
	      return std::span<const _Tp>();
      }
    }

TEST_CASE("reflex.core.views.cartesian_product")
// int main()
{
  static constexpr auto v = std::array{0, 1};
  {
    std::println(" ===== cartesian_product<2> static array =====");
    static constexpr auto rng = v | views::cartesian_product<2>;
    static_assert(std::ranges::size(rng) == 4);
    // static_assert(*std::ranges::begin(rng) == std::array{0, 0});
    std::println("rng + 1 = {}", *std::ranges::next(std::ranges::begin(rng), 1));
    // static_assert(*(std::ranges::begin(rng) + 1) == std::array{0, 1});
    // static_assert(rng[1] == std::array{1, 0});

    static constexpr auto arr = std::meta::reflect_constant_array(rng);
    
    static_assert([:arr:][0][0] == 0);
    static_assert([:arr:][0][1] == 0);
    // static_assert([:arr:][1][0] == 1);
    // static_assert([:arr:][1][1] == 0);
    std::println("arr = {}", [:arr:]);

    using RangeT = decltype(rng);
    using ArrT = decltype(arr);
    // std::println("ArrT = {}", display_string_of(type_of(arr)));
    // static_assert(is_array_type(type_of(arr)));
    
    static_assert(std::ranges::input_range<RangeT>);
    using RangeValueT = std::ranges::range_value_t<RangeT>;
    std::println("RangeValueT = {}", display_string_of(dealias(^^RangeValueT)));
    // static_assert(std::meta::is_array_type(^^ArrT));
    // static_assert(std::is_array_v<std::array<int, 2>>);
    // static_assert(std::meta::is_array_type(^^std::array<int, 2>));
    // static_assert(std::meta::is_array_type(^^RangeValueT));

    static constexpr auto test1 = std::define_static_array(std::views::iota(0uz, 4uz));
    static_assert(test1[0] == 0);
    static_assert(test1[1] == 1);
    static_assert(test1[2] == 2);
    static_assert(test1[3] == 3);
    static_assert(test1.size() == 4);

    static constexpr auto p2 = define_static_array(v | views::cartesian_product<2>);
    static constexpr auto p2_span = test_define_static_array(v | views::cartesian_product<2>);
    // static_assert(p2_span[1][0] == 1); // fails

    template for(constexpr auto p : p2)
    {
      std::println(" - {}", p);
    }

    std::println(" ===== cartesian_product<2> in loop =====");

    template for(constexpr auto p : v | views::cartesian_product<2>)
    {
      std::println(" - {}", p);
    }
    static_assert(p2.size() == 4);
    // static_assert(p2[0] == std::array{0, 0});
    // static_assert(p2[1][0] == 1); // fails
    // static_assert(p2[1][1] == 0);
    // static_assert(p2[1] == std::array{1, 0});
    // static_assert(p2[2] == std::array{0, 1});
    // static_assert(p2[3] == std::array{1, 1});
  }
  std::println(" ===== cartesian_product<3> =====");
  for(const auto& p : v | views::cartesian_product<3>)
  {
    std::println(" - {}", p);
  }
  std::println(" ===== cartesian_product<4> =====");
  for(const auto& p : v | views::cartesian_product<4>)
  {
    std::println(" - {}", p);
  }
}
